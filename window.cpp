#include "window.h"
#include "ui.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawUI(hwnd, hdc);
        EndPaint(hwnd, &ps);
    } break;

    case WM_LBUTTONDOWN:
        OnLeftButtonDown(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;

    case WM_MOUSEMOVE:
        OnMouseMove(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;

    case WM_MOUSELEAVE:
        OnMouseLeave(hwnd);
        break;

    case WM_TIMER:
        OnTimer(hwnd, wParam);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
