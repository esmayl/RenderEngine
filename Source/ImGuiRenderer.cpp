#include "ImGuiRenderer.h"
#include <d3dcompiler.h>

struct VSConstants { float mvp[4][4]; };

bool ImGuiRenderer::Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context)
{
    m_hwnd = hwnd;
    m_device = device;
    m_ctx = context;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    UpdateFontTexture();
    CreateDeviceObjects();
    return true;
}

void ImGuiRenderer::Shutdown()
{
    InvalidateDeviceObjects();
    if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
}

void ImGuiRenderer::NewFrame(float displayWidth, float displayHeight, double deltaTime)
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(displayWidth, displayHeight);
    io.DeltaTime = (deltaTime > 0.0) ? (float)deltaTime : 1.0f / 60.0f;
    // Mouse position is polled from Win32 cursor (new input API)
    POINT p; GetCursorPos(&p); ScreenToClient(m_hwnd, &p);
    io.AddMousePosEvent((float)p.x, (float)p.y);
    // Begin new frame
    ImGui::NewFrame();
}

void ImGuiRenderer::Render()
{
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || drawData->CmdListsCount == 0) return;

    // Create/update buffers
    int totalVtx = 0, totalIdx = 0;
    for (int n = 0; n < drawData->CmdListsCount; n++)
    { totalVtx += drawData->CmdLists[n]->VtxBuffer.Size; totalIdx += drawData->CmdLists[n]->IdxBuffer.Size; }

    if (!m_vb || m_vbSize < totalVtx)
    {
        if (m_vb) m_vb->Release();
        m_vbSize = totalVtx + 5000;
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DYNAMIC; desc.ByteWidth = m_vbSize * sizeof(ImDrawVert);
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER; desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_device->CreateBuffer(&desc, nullptr, &m_vb);
    }
    if (!m_ib || m_ibSize < totalIdx)
    {
        if (m_ib) m_ib->Release();
        m_ibSize = totalIdx + 10000;
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DYNAMIC; desc.ByteWidth = m_ibSize * sizeof(ImDrawIdx);
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER; desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_device->CreateBuffer(&desc, nullptr, &m_ib);
    }

    // Upload
    D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;
    m_ctx->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource);
    m_ctx->Map(m_ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource);
    ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource.pData;
    ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource.pData;
    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = drawData->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }
    m_ctx->Unmap(m_vb, 0);
    m_ctx->Unmap(m_ib, 0);

    // Setup render state: alpha blend, scissor, texture, shaders
    UINT stride = sizeof(ImDrawVert);
    UINT offset = 0;
    m_ctx->IASetInputLayout(m_layout);
    m_ctx->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
    m_ctx->IASetIndexBuffer(m_ib, sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
    m_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_ctx->VSSetShader(m_vs, nullptr, 0);
    m_ctx->PSSetShader(m_ps, nullptr, 0);
    m_ctx->PSSetSamplers(0, 1, &m_sampler);
    m_ctx->OMSetBlendState(m_blend, nullptr, 0xFFFFFFFF);
    m_ctx->OMSetDepthStencilState(m_depth, 0);
    m_ctx->RSSetState(m_raster);

    // Setup projection matrix
    const float L = drawData->DisplayPos.x;
    const float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
    const float T = drawData->DisplayPos.y;
    const float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
    VSConstants vsconst = { {
        {  2.0f/(R-L),  0.0f,        0.0f, 0.0f },
        {  0.0f,        2.0f/(T-B),  0.0f, 0.0f },
        {  0.0f,        0.0f,        0.5f, 0.0f },
        { (R+L)/(L-R), (T+B)/(B-T),  0.5f, 1.0f },
    } };
    m_ctx->UpdateSubresource(m_cb, 0, nullptr, &vsconst, 0, 0);
    m_ctx->VSSetConstantBuffers(0, 1, &m_cb);

    // Render command lists
    int vtx_offset = 0;
    int idx_offset = 0;
    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = drawData->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            { pcmd->UserCallback(cmd_list, pcmd); }
            else
            {
                const D3D11_RECT r = { (LONG)(pcmd->ClipRect.x), (LONG)(pcmd->ClipRect.y), (LONG)(pcmd->ClipRect.z), (LONG)(pcmd->ClipRect.w) };
                m_ctx->RSSetScissorRects(1, &r);
                ID3D11ShaderResourceView* tex = m_fontSRV;
                if (pcmd->GetTexID() != 0)
                    tex = (ID3D11ShaderResourceView*)(intptr_t)pcmd->GetTexID();
                m_ctx->PSSetShaderResources(0, 1, &tex);
                m_ctx->DrawIndexed(pcmd->ElemCount, idx_offset, vtx_offset);
            }
            idx_offset += pcmd->ElemCount;
        }
        vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Restore a neutral D3D11 state so app rendering isn't constrained
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ID3D11SamplerState* nullSamp = nullptr;
    m_ctx->PSSetShaderResources(0, 1, &nullSRV);
    m_ctx->PSSetSamplers(0, 1, &nullSamp);
    m_ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    m_ctx->OMSetDepthStencilState(nullptr, 0);
    m_ctx->RSSetState(nullptr);
    m_ctx->IASetInputLayout(nullptr);
    m_ctx->VSSetShader(nullptr, nullptr, 0);
    m_ctx->PSSetShader(nullptr, nullptr, 0);
}

void ImGuiRenderer::ProcessWin32Event(UINT msg, WPARAM wParam, LPARAM lParam)
{
    ImGuiIO& io = ImGui::GetIO();
    switch (msg)
    {
        case WM_LBUTTONDOWN: io.AddMouseButtonEvent(0, true); break;
        case WM_LBUTTONUP:   io.AddMouseButtonEvent(0, false); break;
        case WM_RBUTTONDOWN: io.AddMouseButtonEvent(1, true); break;
        case WM_RBUTTONUP:   io.AddMouseButtonEvent(1, false); break;
        case WM_MBUTTONDOWN: io.AddMouseButtonEvent(2, true); break;
        case WM_MBUTTONUP:   io.AddMouseButtonEvent(2, false); break;
        case WM_MOUSEWHEEL:  io.AddMouseWheelEvent(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA); break;
        case WM_MOUSEHWHEEL: io.AddMouseWheelEvent((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, 0.0f); break;
        case WM_CHAR:        io.AddInputCharacterUTF16((ImWchar)wParam); break;
        default: break;
    }
}

void ImGuiRenderer::CreateDeviceObjects()
{
    // Create states
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    m_device->CreateSamplerState(&sd, &m_sampler);

    D3D11_BLEND_DESC bd = {};
    bd.AlphaToCoverageEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_device->CreateBlendState(&bd, &m_blend);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.ScissorEnable = TRUE;
    m_device->CreateRasterizerState(&rd, &m_raster);

    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = FALSE; dd.StencilEnable = FALSE;
    m_device->CreateDepthStencilState(&dd, &m_depth);

    // Constant buffer
    D3D11_BUFFER_DESC cbd = {};
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.ByteWidth = sizeof(VSConstants);
    cbd.Usage = D3D11_USAGE_DEFAULT;
    m_device->CreateBuffer(&cbd, nullptr, &m_cb);

    // Input layout matches ImDrawVert
    D3D11_INPUT_ELEMENT_DESC ied[3] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,0, offsetof(ImDrawVert, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // Create shaders from compiled CSO (compiled by CMake fxc rule)
    ID3DBlob* vsb = nullptr; ID3DBlob* psb = nullptr; ID3DBlob* err = nullptr;
    // We rely on Utilities to load precompiled CSO
    // However for simplicity, we use D3DCompileFromFile here for local compile if available
    // In this codebase, shaders are compiled externally; input layout only needs a blob.
    // We'll load the CSO via D3DReadFileToBlob.
    D3DReadFileToBlob(L"ImGuiVertexShader.cso", &vsb);
    D3DReadFileToBlob(L"ImGuiPixelShader.cso", &psb);
    m_device->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &m_vs);
    m_device->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &m_ps);
    m_device->CreateInputLayout(ied, 3, vsb->GetBufferPointer(), vsb->GetBufferSize(), &m_layout);
    if (vsb) vsb->Release(); if (psb) psb->Release(); if (err) err->Release();
}

void ImGuiRenderer::InvalidateDeviceObjects()
{
    if (m_vb) { m_vb->Release(); m_vb = nullptr; }
    if (m_ib) { m_ib->Release(); m_ib = nullptr; }
    if (m_vs) { m_vs->Release(); m_vs = nullptr; }
    if (m_ps) { m_ps->Release(); m_ps = nullptr; }
    if (m_layout){ m_layout->Release(); m_layout = nullptr; }
    if (m_cb) { m_cb->Release(); m_cb = nullptr; }
    if (m_sampler){ m_sampler->Release(); m_sampler = nullptr; }
    if (m_fontSRV){ m_fontSRV->Release(); m_fontSRV = nullptr; }
    if (m_blend){ m_blend->Release(); m_blend = nullptr; }
    if (m_raster){ m_raster->Release(); m_raster = nullptr; }
    if (m_depth){ m_depth->Release(); m_depth = nullptr; }
}

void ImGuiRenderer::UpdateFontTexture()
{
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr; int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w; desc.Height = h; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA srd = { pixels, (UINT)(w * 4), 0 };
    ID3D11Texture2D* tex = nullptr;
    m_device->CreateTexture2D(&desc, &srd, &tex);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = desc.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(tex, &srvd, &m_fontSRV);
    if (tex) tex->Release();

    io.Fonts->SetTexID((ImTextureID)(intptr_t)m_fontSRV);
}
