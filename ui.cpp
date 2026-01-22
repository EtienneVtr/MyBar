#include "ui.h"
#include "app_loader.h"

#include <windowsx.h>
#include <shellapi.h>

#include <wingdi.h>

#include <cmath>

#pragma comment(lib, "msimg32.lib")

RECT closeBtnRect{};
int closeAlpha = 0;
bool closeTargetVisible = false;

int reflectAlpha = 0;
int reflectTargetAlpha = 0;

static bool trackingMouseLeave = false;

static void DrawTopHighlight(HDC hdc, int w, int h, int alpha) {
    if (alpha <= 0) return;

    int hh = (int)(h * 0.38f); // hauteur de highlight
    if (hh < 6) hh = 6;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -hh;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP dib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(memDC, dib);

    auto* px = (unsigned int*)bits;

    for (int y = 0; y < hh; y++) {
        // fort tout en haut, puis fade très rapide
        float t = 1.0f - (float)y / (float)(hh - 1);
        t = powf(t, 2.2f);

        int a = (int)(alpha * t);
        if (a > 255) a = 255;

        unsigned int v = (unsigned int)((a << 24) | (a << 16) | (a << 8) | a);
        for (int x = 0; x < w; x++) px[y * w + x] = v;
    }

    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(hdc, 0, 0, w, hh, memDC, 0, 0, w, hh, bf);

    SelectObject(memDC, old);
    DeleteObject(dib);
    DeleteDC(memDC);
}

static void DrawBevel(HDC hdc, int w, int h) {
    HPEN light = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
    HPEN dark = CreatePen(PS_SOLID, 1, RGB(35, 35, 35));

    HGDIOBJ old = SelectObject(hdc, light);

    // haut + gauche
    MoveToEx(hdc, 1, 1, NULL); LineTo(hdc, w - 2, 1);
    MoveToEx(hdc, 1, 1, NULL); LineTo(hdc, 1, h - 2);

    SelectObject(hdc, dark);

    // bas + droite
    MoveToEx(hdc, 1, h - 2, NULL); LineTo(hdc, w - 2, h - 2);
    MoveToEx(hdc, w - 2, 1, NULL); LineTo(hdc, w - 2, h - 2);

    SelectObject(hdc, old);
    DeleteObject(light);
    DeleteObject(dark);
}

void DrawUI(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

    int barW = rc.right - rc.left;
    int barH = rc.bottom - rc.top;

    // 1) highlight du haut (effet verre)
    DrawTopHighlight(hdc, barW, barH, 70);

    // 3) icônes
    for (auto& app : apps) {
        DrawIconEx(hdc, app.rect.left, app.rect.top, app.icon, iconSize, iconSize, 0, NULL, DI_NORMAL);
    }

    // 4) bouton close
    if (closeAlpha > 0) {
        int a = closeAlpha;
        COLORREF col = RGB((120 * a) / 255, (120 * a) / 255, (120 * a) / 255);

        HPEN hPen = CreatePen(PS_SOLID, 2, col);
        HGDIOBJ oldPen2 = SelectObject(hdc, hPen);

        MoveToEx(hdc, closeBtnRect.left, closeBtnRect.top, NULL);
        LineTo(hdc, closeBtnRect.right, closeBtnRect.bottom);

        MoveToEx(hdc, closeBtnRect.right, closeBtnRect.top, NULL);
        LineTo(hdc, closeBtnRect.left, closeBtnRect.bottom);

        SelectObject(hdc, oldPen2);
        DeleteObject(hPen);
    }

    // 5) bevel / rim par-dessus (finish)
    DrawBevel(hdc, barW, barH);
}

void OnLeftButtonDown(HWND hwnd, int x, int y) {
    POINT pt = { x, y };

    for (auto& app : apps) {
        if (PtInRect(&app.rect, pt)) {
            ShellExecute(nullptr, L"open", app.path.c_str(), nullptr, nullptr, SW_SHOW);
        }
    }

    if (PtInRect(&closeBtnRect, pt)) {
        SendMessage(hwnd, WM_CLOSE, 0, 0);
    }
}

void OnMouseMove(HWND hwnd, int x, int y) {
    // déclenche WM_MOUSELEAVE une fois
    if (!trackingMouseLeave) {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        trackingMouseLeave = true;
    }
    
    POINT pt = { x, y };

    bool hoverClose = PtInRect(&closeBtnRect, pt);
    if (hoverClose != closeTargetVisible) {
        closeTargetVisible = hoverClose;
        SetTimer(hwnd, TIMER_CLOSE_FADE, 16, NULL);
    }

    InvalidateRect(hwnd, NULL, FALSE);
}

void OnTimer(HWND hwnd, WPARAM wParam) {
    if (wParam == TIMER_CLOSE_FADE) {
        const int speed = 40;
        if (closeTargetVisible) {
            closeAlpha += speed; if (closeAlpha >= 255) closeAlpha = 255;
        }
        else {
            closeAlpha -= speed; if (closeAlpha <= 0) closeAlpha = 0;
        }
        if (closeAlpha == (closeTargetVisible ? 255 : 0)) KillTimer(hwnd, TIMER_CLOSE_FADE);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    if (wParam == TIMER_HOVER_ANIM) {
        const int speed = 25; // vitesse du reflet
        if (reflectAlpha < reflectTargetAlpha) {
            reflectAlpha += speed;
            if (reflectAlpha > reflectTargetAlpha) reflectAlpha = reflectTargetAlpha;
        }
        else if (reflectAlpha > reflectTargetAlpha) {
            reflectAlpha -= speed;
            if (reflectAlpha < reflectTargetAlpha) reflectAlpha = reflectTargetAlpha;
        }

        if (reflectAlpha == reflectTargetAlpha) {
            KillTimer(hwnd, TIMER_HOVER_ANIM);
        }


        InvalidateRect(hwnd, NULL, FALSE);
    }
}
