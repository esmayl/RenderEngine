#pragma once
#include <ostream>
#include <iostream>
#include <windows.h>
#include <windef.h>
#include <thread>   // For std::this_thread::sleep_for

#include "Utilities.h"
#include "RendererEngine2D.h"

#pragma comment(lib, "winmm.lib") // Instruction to the Microsoft compiler, used to link winm.lib after compiling to .obj

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
