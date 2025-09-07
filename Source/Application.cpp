#include "Application.h"

#include <Windowsx.h>
#include <chrono>

namespace
{
    const wchar_t kWindowClassName[] = L"RenderEngineWindowClass";
    const wchar_t kWindowTitle[]     = L"Render Engine";
} // namespace

int Application::run( HINSTANCE hInstance, int nCmdShow )
{
    timeBeginPeriod( 1 );

    if ( !registerWindowClass( hInstance ) )
    {
        timeEndPeriod( 1 );
        return 0;
    }

    if ( !createMainWindow( hInstance, nCmdShow ) )
    {
        timeEndPeriod( 1 );
        return 0;
    }

    MSG msg{};
    bool quit = false;

    while ( !quit )
    {
        while ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
        {
            if ( msg.message == WM_QUIT )
            {
                quit = true;
            }
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        if ( quit )
            break;
        InvalidateRect( hwnd_, nullptr, FALSE );
    }

    timeEndPeriod( 1 );
    return 0;
}

LRESULT CALLBACK Application::staticWindowProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    if ( msg == WM_NCCREATE )
    {
        auto cs  = reinterpret_cast<CREATESTRUCT*>( lParam );
        auto app = reinterpret_cast<Application*>( cs->lpCreateParams );
        SetWindowLongPtr( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( app ) );
        return TRUE;
    }

    auto app = reinterpret_cast<Application*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
    if ( app )
    {
        return app->windowProc( hwnd, msg, wParam, lParam );
    }
    return DefWindowProc( hwnd, msg, wParam, lParam );
}

LRESULT Application::windowProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    renderer_.ProcessEvent( msg, wParam, lParam );

    switch ( msg )
    {
    case WM_CREATE:
        try
        {
            renderer_.Init( hwnd, blockWidth_, blockHeight_ );
        }
        catch ( const std::exception& ex )
        {
            MessageBoxA( hwnd, ex.what(), "Initialization Error", MB_ICONERROR | MB_OK );
            renderer_.OnShutdown();
            PostQuitMessage( 1 );
        }
        catch ( ... )
        {
            MessageBoxA( hwnd, "Unknown initialization error", "Initialization Error", MB_ICONERROR | MB_OK );
            renderer_.OnShutdown();
            PostQuitMessage( 1 );
        }
        return 0;
    case WM_DESTROY:
        renderer_.OnShutdown();
        PostQuitMessage( 0 );
        return 0;
    case WM_SIZE:
    {
        const UINT width  = LOWORD( lParam );
        const UINT height = HIWORD( lParam );
        try
        {
            renderer_.OnResize( static_cast<int>( width ), static_cast<int>( height ) );
        }
        catch ( const std::exception& ex )
        {
            MessageBoxA( hwnd, ex.what(), "Resize Error", MB_ICONERROR | MB_OK );
            renderer_.OnShutdown();
            PostQuitMessage( 1 );
        }
        catch ( ... )
        {
            MessageBoxA( hwnd, "Unknown resize error", "Resize Error", MB_ICONERROR | MB_OK );
            renderer_.OnShutdown();
            PostQuitMessage( 1 );
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        const int x   = GET_X_LPARAM( lParam );
        const int y   = GET_Y_LPARAM( lParam );
        const int idx = renderer_.FindNearestFoodScreen( x, y, 24.0f );
        if ( idx >= 0 )
        {
            if ( idx == renderer_.GetActiveFoodIndex() )
            {
                renderer_.SetActiveFoodByIndex( -1 );
            }
            else
            {
                renderer_.SetActiveFoodByIndex( idx );
            }
        }
        return 0;
    }
    case WM_KEYUP:
    {
        if ( wParam == 'R' )
        {
            renderer_.ResetGame();
            renderer_.StartStage( 1 );
            return 0;
        }
        if ( wParam == 'N' )
        {
            renderer_.AdvanceStage();
            return 0;
        }
        if ( wParam == 'E' )
        {
            renderer_.ToggleEndless( true );
            return 0;
        }
        if ( wParam == '1' || wParam == VK_NUMPAD1 )
        {
            renderer_.ApplyUpgrade( 1 );
            return 0;
        }
        if ( wParam == '2' || wParam == VK_NUMPAD2 )
        {
            renderer_.ApplyUpgrade( 2 );
            return 0;
        }
        if ( wParam == '3' || wParam == VK_NUMPAD3 )
        {
            renderer_.ApplyUpgrade( 3 );
            return 0;
        }
        return 0;
    }
    case WM_RBUTTONUP:
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint( hwnd, &ps );
        try
        {
            renderer_.OnPaint( hwnd );
        }
        catch ( const std::exception& ex )
        {
            EndPaint( hwnd, &ps );
            MessageBoxA( hwnd, ex.what(), "Render Error", MB_ICONERROR | MB_OK );
            renderer_.OnShutdown();
            PostQuitMessage( 1 );
            return 0;
        }
        catch ( ... )
        {
            EndPaint( hwnd, &ps );
            MessageBoxA( hwnd, "Unknown render error", "Render Error", MB_ICONERROR | MB_OK );
            renderer_.OnShutdown();
            PostQuitMessage( 1 );
            return 0;
        }
        EndPaint( hwnd, &ps );
        return 0;
    }
    default:
        return DefWindowProc( hwnd, msg, wParam, lParam );
    }
}

bool Application::registerWindowClass( HINSTANCE hInstance )
{
    WNDCLASS wc{};
    wc.lpfnWndProc   = &Application::staticWindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = kWindowClassName;
    return RegisterClass( &wc ) != 0;
}

bool Application::createMainWindow( HINSTANCE hInstance, int nCmdShow )
{
    hwnd_ = CreateWindowEx( 0, kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280,
                            720, nullptr, nullptr, hInstance, this );

    if ( !hwnd_ )
        return false;
    ShowWindow( hwnd_, nCmdShow );
    return true;
}
