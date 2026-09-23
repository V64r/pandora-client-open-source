#include "../injector/injector.h"
#include "gui.h"
#include <windows.h>
#include <string>
#include <memory>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (gui::instance && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_NCHITTEST:
    {
        LRESULT hit = DefWindowProc(hwnd, msg, wparam, lparam);
        if (hit == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (pt.y < 30) {
                return HTCAPTION;
            }
        }
        return hit;
    }
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"LoaderWindow";

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, 
        L"LoaderWindow",
        L"Loader",
        WS_POPUP | WS_VISIBLE,
        0, 0,
        730, 460,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create window!", L"Error", MB_ICONERROR);
        return 1;
    }

    RECT rc;
    GetWindowRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, NULL,
        (screenWidth - width) / 2,
        (screenHeight - height) / 2,
        0, 0,
        SWP_NOSIZE | SWP_NOZORDER
    );

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    gui::instance = std::make_unique<gui::c_gui>();
    if (!gui::instance->initialize(hwnd)) {
        MessageBoxW(nullptr, L"Failed to initialize GUI!", L"Error", MB_ICONERROR);
        return 1;
    }

    UINT_PTR timerId = SetTimer(hwnd, 1, 100, NULL);

    MSG msg = {};
    bool running = true;

    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }

            if (msg.message == WM_ACTIVATEAPP ||
                msg.message == WM_SETFOCUS ||
                msg.message == WM_KILLFOCUS) {
                if (gui::instance) {
                    gui::instance->force_refresh = true;
                }
            }
        }

        if (!running) break;

        if (gui::instance) {
            gui::instance->check_window_focus();
        }

        if (gui::instance->should_close) {
            running = false;
            break;
        }

        gui::instance->render();

        if (gui::instance->swap_chain) {
            gui::instance->swap_chain->Present(1, 0);
        }
    }

    KillTimer(hwnd, timerId);

    gui::instance->shutdown();
    gui::instance.reset();

    return 0;
}