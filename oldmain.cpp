#define UNICODE
#define _UNICODE
#include <windows.h>
#include <dwmapi.h>
#include <vector>
#include <string>
#include <shlobj.h>
#include <Shobjidl.h>
#include <atlbase.h>
#include <windowsx.h>
#include <debugapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#define TIMER_CLOSE_FADE 1

// --- Structures API non documentées ---
enum WINDOWCOMPOSITIONATTRIB { WCA_ACCENT_POLICY = 19 };
struct ACCENT_POLICY { int AccentState; int AccentFlags; int GradientColor; int AnimationId; };
struct WINDOWCOMPOSITIONATTRIBDATA { int Attrib; PVOID pvData; SIZE_T cbData; };
typedef BOOL(WINAPI* pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

struct AppIcon { RECT rect; std::wstring path; HICON icon; };
std::vector<AppIcon> apps;
const int iconSize = 32;
const int spacing = 15;

RECT closeBtnRect;
int closeAlpha = 0;
bool closeTargetVisible = false;

std::wstring ResolveShortcut(const std::wstring& lnkPath) {
    CComPtr<IShellLink> psl;
    wchar_t target[MAX_PATH] = { 0 };
    if (SUCCEEDED(psl.CoCreateInstance(CLSID_ShellLink))) {
        CComPtr<IPersistFile> ppf;
        if (SUCCEEDED(psl.QueryInterface(&ppf))) {
            if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ))) {
                psl->GetPath(target, MAX_PATH, nullptr, 0);
            }
        }
    }
    return target;
}

void LoadAppsFromFolder(const std::wstring& folder) {
    WIN32_FIND_DATA ffd;
    // Correction du chemin pour FindFirstFile
    std::wstring searchPath = folder + L"\\*";
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &ffd);

    if (hFind == INVALID_HANDLE_VALUE) {
        MessageBox(NULL, (L"Dossier introuvable : " + folder).c_str(), L"Erreur", MB_ICONERROR);
        return;
    }

    int x = spacing;
    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring filePath = folder + L"\\" + ffd.cFileName;
            if (filePath.ends_with(L".lnk")) {
                std::wstring resolved = ResolveShortcut(filePath);
                if (!resolved.empty()) filePath = resolved;
            }

            SHFILEINFO sfi = { 0 };
            if (SHGetFileInfo(filePath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON)) {
                AppIcon app;
                app.rect = { x, 10, x + iconSize, 10 + iconSize };
                app.path = filePath;
                app.icon = sfi.hIcon;
                apps.push_back(app);
                x += iconSize + spacing;
            }
        }
    } while (FindNextFile(hFind, &ffd) != 0);
    FindClose(hFind);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1; // Ne rien dessiner en fond pour éviter l'opacité

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        for (auto& app : apps) {
            DrawIconEx(hdc, app.rect.left, app.rect.top, app.icon, iconSize, iconSize, 0, NULL, DI_NORMAL);
        }

        if (closeAlpha > 0) {
            int a = closeAlpha;
            COLORREF col = RGB(
                (120 * a) / 255,
                (120 * a) / 255,
                (120 * a) / 255
            );

            HPEN hPen = CreatePen(PS_SOLID, 2, col);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);

            MoveToEx(hdc, closeBtnRect.left, closeBtnRect.top, NULL);
            LineTo(hdc, closeBtnRect.right, closeBtnRect.bottom);

            MoveToEx(hdc, closeBtnRect.right, closeBtnRect.top, NULL);
            LineTo(hdc, closeBtnRect.left, closeBtnRect.bottom);

            SelectObject(hdc, oldPen);
            DeleteObject(hPen);
        }

        EndPaint(hwnd, &ps);
    } break;

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        for (auto& app : apps) {
            if (PtInRect(&app.rect, pt)) {
                ShellExecute(nullptr, L"open", app.path.c_str(), nullptr, nullptr, SW_SHOW);
            }
        }

        if (PtInRect(&closeBtnRect, pt)) {
            SendMessage(hwnd, WM_CLOSE, 0, 0); // Ferme proprement
        }
    } break;

    case WM_TIMER:
        if (wParam == TIMER_CLOSE_FADE) {
            const int speed = 40; // vitesse du fade

            // Close bouton
            if (closeTargetVisible) {
                closeAlpha += speed;
                if (closeAlpha >= 255) closeAlpha = 255;
            }
            else {
                closeAlpha -= speed;
                if (closeAlpha <= 0) closeAlpha = 0;
            }

            // Arrête le timer si tous les alphas ont atteint leur cible
            if (closeAlpha == (closeTargetVisible ? 255 : 0)) {
                KillTimer(hwnd, TIMER_CLOSE_FADE);
            }

            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        // Close button
        bool hoverClose = PtInRect(&closeBtnRect, pt);
        if (hoverClose != closeTargetVisible) {
            closeTargetVisible = hoverClose;
            SetTimer(hwnd, TIMER_CLOSE_FADE, 16, NULL); // timer unique
        }
    } break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    CoInitialize(nullptr);

    LoadAppsFromFolder(L"C:\\Users\\etien\\ProjetsPerso\\mybar\\apps");

    int numApps = 0;

    if (not apps.empty()) {
        numApps = (int)apps.size();
    }

    int btnSize = 6;

    int barWidth = (numApps * iconSize) + ((numApps + 1) * spacing);

    int barHeight = iconSize + 20;

    // On le place pour qu'il "morde" un peu sur le bord droit
    closeBtnRect.left = barWidth - 12;
    closeBtnRect.top = 6; // On le fait dépasser un peu vers le haut (style badge flottant)
    closeBtnRect.right = closeBtnRect.left + btnSize;
    closeBtnRect.bottom = closeBtnRect.top + btnSize;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    int posX = (screenWidth - barWidth) / 2; // Centré horizontalement
    int posY = screenHeight - barHeight - 150; // À 20px du bas de l'écran

    // --- CRÉATION DE LA FENÊTRE ---
    const wchar_t CLASS_NAME[] = L"MyBarClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, // TOOLWINDOW évite l'apparition dans Alt-Tab
        CLASS_NAME, L"MyBar",
        WS_POPUP,
        posX, posY, barWidth, barHeight, // Utilisation des variables calculées
        nullptr, nullptr, hInst, nullptr
    );

    // 2. UTILISER LWA_ALPHA (255) au lieu de COLORKEY
    // Cela permet aux icônes d'avoir des bords lisses (anti-aliasing)
    // SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    // 3. APPLIQUER LE FLOU (Version BlurBehind)
    auto fn = (pSetWindowCompositionAttribute)GetProcAddress(GetModuleHandle(L"user32.dll"), "SetWindowCompositionAttribute");
    if (fn) {
        ACCENT_POLICY policy = {};
        policy.AccentState = 2;
        policy.AccentFlags = 2;
        policy.GradientColor = 0x30FFFFFF;

        WINDOWCOMPOSITIONATTRIBDATA data = { WCA_ACCENT_POLICY, &policy, sizeof(policy) };
        fn(hwnd, &data);
    }

    // 4. ÉTENDRE LE CADRE (Indispensable pour que le DWM "remplisse" le vide avec du flou)
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