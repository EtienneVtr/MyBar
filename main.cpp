#include <windows.h>
#include <dwmapi.h>

#include "app_loader.h"
#include "ui.h"
#include "window.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

// --- Structures API non documentées ---
enum WINDOWCOMPOSITIONATTRIB { WCA_ACCENT_POLICY = 19 };
struct ACCENT_POLICY { int AccentState; int AccentFlags; int GradientColor; int AnimationId; };
struct WINDOWCOMPOSITIONATTRIBDATA { int Attrib; PVOID pvData; SIZE_T cbData; };
typedef BOOL(WINAPI* pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    CoInitialize(nullptr);

    LoadAppsFromFolder(L"C:\\Users\\etien\\ProjetsPerso\\mybar\\apps");

    int numApps = (int)apps.size();
    int btnSize = 6;

    int barWidth = (numApps * iconSize) + ((numApps + 1) * spacing);
    int barHeight = iconSize + 20;

    closeBtnRect.left = barWidth - 12;
    closeBtnRect.top = 6;
    closeBtnRect.right = closeBtnRect.left + btnSize;
    closeBtnRect.bottom = closeBtnRect.top + btnSize;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    int posX = (screenWidth - barWidth) / 2;
    int posY = screenHeight - barHeight - 150;

    const wchar_t CLASS_NAME[] = L"MyBarClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME, L"MyBar",
        WS_POPUP,
        posX, posY, barWidth, barHeight,
        nullptr, nullptr, hInst, nullptr
    );

    auto fn = (pSetWindowCompositionAttribute)GetProcAddress(
        GetModuleHandle(L"user32.dll"),
        "SetWindowCompositionAttribute"
    );

    if (fn) {
        ACCENT_POLICY policy = {};
        policy.AccentState = 2;
        policy.AccentFlags = 2;
        policy.GradientColor = 0x30FFFFFF;

        WINDOWCOMPOSITIONATTRIBDATA data = { WCA_ACCENT_POLICY, &policy, sizeof(policy) };
        fn(hwnd, &data);
    }

    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    BOOL isDark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &isDark, sizeof(isDark));

    int cornerPreference = 2;
    DwmSetWindowAttribute(hwnd, 33, &cornerPreference, sizeof(cornerPreference));

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return 0;
}
