// Refactored entry to a cohesive Application class
#include "Application.h"

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow )
{
    Application app;
    return app.run( hInstance, nCmdShow );
}
