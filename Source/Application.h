#pragma once

#include "InstancedRendererEngine2D.h"

#include <mmsystem.h>
#include <windows.h>

#pragma comment( lib, "winmm.lib" )

class Application
{
  public:
    Application()                                = default;
    Application( const Application& )            = delete;
    Application& operator=( const Application& ) = delete;
    Application( Application&& )                 = delete;
    Application& operator=( Application&& )      = delete;
    ~Application()                               = default;

    int run( HINSTANCE hInstance, int nCmdShow );

  private:
    static LRESULT CALLBACK staticWindowProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
    LRESULT windowProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );

    bool registerWindowClass( HINSTANCE hInstance );
    bool createMainWindow( HINSTANCE hInstance, int nCmdShow );

    HWND hwnd_       = nullptr;
    int blockWidth_  = 1;
    int blockHeight_ = 1;
    InstancedRendererEngine2D renderer_;
};
