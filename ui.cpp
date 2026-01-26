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

static void DrawTint(HDC hdc, int w, int h, int alpha /*0..255*/, BYTE gray /*ex: 20..80*/) {
    if (alpha <= 0) return;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP dib = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(mem, dib);

    unsigned int* px = (unsigned int*)bits;
    // premultiplied: channel = gray * alpha / 255
    int c = (gray * alpha) / 255;
    unsigned int v = (unsigned int)((alpha << 24) | (c << 16) | (c << 8) | c);
    for (int i = 0; i < w * h; i++) px[i] = v;

    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(hdc, 0, 0, w, h, mem, 0, 0, w, h, bf);

    SelectObject(mem, old);
    DeleteObject(dib);
    DeleteDC(mem);
}

void DrawUI(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    // 1) Backbuffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    DrawTint(memDC, w, h, 120, 15);

    DrawTopHighlight(memDC, w, h, 70);

    for (auto& app : apps) {
        DrawIconEx(memDC, app.rect.left, app.rect.top, app.icon, iconSize, iconSize, 0, NULL, DI_NORMAL);
    }

    if (closeAlpha > 0) {
        int a = closeAlpha;
        COLORREF col = RGB((120 * a) / 255, (120 * a) / 255, (120 * a) / 255);

        HPEN hPen = CreatePen(PS_SOLID, 2, col);
        HGDIOBJ oldPen2 = SelectObject(memDC, hPen);

        MoveToEx(memDC, closeBtnRect.left, closeBtnRect.top, NULL);
        LineTo(memDC, closeBtnRect.right, closeBtnRect.bottom);

        MoveToEx(memDC, closeBtnRect.right, closeBtnRect.top, NULL);
        LineTo(memDC, closeBtnRect.left, closeBtnRect.bottom);

        SelectObject(memDC, oldPen2);
        DeleteObject(hPen);
    }

    DrawBevel(memDC, w, h);

    // 3) Copie finale (1 seul blit => plus de tressautement)
    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    // 4) Cleanup
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
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
    POINT pt = { x, y };

    bool hoverClose = PtInRect(&closeBtnRect, pt);
    if (hoverClose != closeTargetVisible) {
        closeTargetVisible = hoverClose;
        SetTimer(hwnd, TIMER_CLOSE_FADE, 16, NULL);
        InvalidateRect(hwnd, NULL, FALSE);
    }
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
}
