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

bool barHover = false;
int mouseX = 0;
int mouseY = 0;
int reflectAlpha = 0;
int reflectTargetAlpha = 0;

static bool trackingMouseLeave = false;

static float clamp01(float v) { return (v < 0.f) ? 0.f : (v > 1.f ? 1.f : v); }

static void DrawReflection(HDC hdc, int barW, int barH, int centerX, int alpha) {
    if (alpha <= 0 || barW <= 0 || barH <= 0) return;

    // on dessine un spot sur toute la barre (plus simple + plus naturel)
    const int w = barW;
    const int h = barH;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC memDC = CreateCompatibleDC(hdc);
    if (!memDC) return;

    HBITMAP dib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        if (dib) DeleteObject(dib);
        DeleteDC(memDC);
        return;
    }

    HGDIOBJ old = SelectObject(memDC, dib);
    auto* px = (unsigned int*)bits;

    // centre du spot = souris (mouseX/mouseY => variables globales)
    float cx = (float)mouseX;
    float cy = (float)mouseY;

    // “rayon” du spot
    const float sx = (float)w * 0.18f;   // largeur (monte si tu veux plus large)
    const float sy = (float)h * 0.55f;   // hauteur (monte si tu veux plus diffus)
    const float inv2sx2 = 1.0f / (2.0f * sx * sx);
    const float inv2sy2 = 1.0f / (2.0f * sy * sy);

    // highlight interne plus petit (le “point chaud”)
    const float sx2 = sx * 0.35f;
    const float sy2 = sy * 0.35f;
    const float inv2sx22 = 1.0f / (2.0f * sx2 * sx2);
    const float inv2sy22 = 1.0f / (2.0f * sy2 * sy2);

    // un petit biais: lumière plus visible en haut (reflet sur verre)
    const float topBoost = 1.15f;

    for (int y = 0; y < h; y++) {
        float up = 1.0f - (float)y / (float)h; // 1 en haut -> 0 en bas
        float yLight = 0.85f + 0.15f * up * topBoost;

        for (int x = 0; x < w; x++) {
            float dx = (float)x - cx;
            float dy = (float)y - cy;

            // gaussien large
            float g1 = expf(-(dx * dx) * inv2sx2 - (dy * dy) * inv2sy2);

            // gaussien petit (point chaud)
            float g2 = expf(-(dx * dx) * inv2sx22 - (dy * dy) * inv2sy22);

            // mix: spot + point chaud
            float intensity = 0.55f * g1 + 0.75f * g2;

            // estompe haut/bas pour se fondre avec la barre (encore plus si besoin)
            float yn = ((float)y - (h * 0.5f)) / (h * 0.5f);
            float vfade = 1.0f - yn * yn;
            if (vfade < 0.f) vfade = 0.f;
            vfade = powf(vfade, 2.2f);

            intensity *= yLight * vfade;
            intensity = clamp01(intensity);

            int a = (int)(alpha * intensity);
            if (a < 0) a = 0;
            if (a > 255) a = 255;

            px[y * w + x] = (unsigned int)((a << 24) | (a << 16) | (a << 8) | a);
        }
    }

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;

    AlphaBlend(hdc, 0, 0, w, h, memDC, 0, 0, w, h, bf);

    SelectObject(memDC, old);
    DeleteObject(dib);
    DeleteDC(memDC);
}

void DrawUI(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

    int barW = rc.right - rc.left;
    int barH = rc.bottom - rc.top;
    DrawReflection(hdc, barW, barH, 0, reflectAlpha);

    HPEN hTop = CreatePen(PS_SOLID, 1, RGB(170, 170, 170));
    HPEN hBot = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));

    HGDIOBJ oldPen = SelectObject(hdc, hTop);
    MoveToEx(hdc, 1, 1, NULL);
    LineTo(hdc, barW - 2, 1);

    SelectObject(hdc, hBot);
    MoveToEx(hdc, 1, barH - 2, NULL);
    LineTo(hdc, barW - 2, barH - 2);

    SelectObject(hdc, oldPen);
    DeleteObject(hTop);
    DeleteObject(hBot);

    for (auto& app : apps) {
        DrawIconEx(hdc, app.rect.left, app.rect.top, app.icon, iconSize, iconSize, 0, NULL, DI_NORMAL);
    }

    if (closeAlpha > 0) {
        int a = closeAlpha;
        COLORREF col = RGB((120 * a) / 255, (120 * a) / 255, (120 * a) / 255);

        HPEN hPen = CreatePen(PS_SOLID, 2, col);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);

        MoveToEx(hdc, closeBtnRect.left, closeBtnRect.top, NULL);
        LineTo(hdc, closeBtnRect.right, closeBtnRect.bottom);

        MoveToEx(hdc, closeBtnRect.right, closeBtnRect.top, NULL);
        LineTo(hdc, closeBtnRect.left, closeBtnRect.bottom);

        SelectObject(hdc, oldPen);
        DeleteObject(hPen);
    }
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
    mouseX = x;
    mouseY = y;

    // déclenche WM_MOUSELEAVE une fois
    if (!trackingMouseLeave) {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        trackingMouseLeave = true;
    }

    // hover barre ON
    barHover = true;
    reflectTargetAlpha = 120; // intensité max du reflet (0..255)
    SetTimer(hwnd, TIMER_HOVER_ANIM, 16, NULL);
    
    POINT pt = { x, y };

    bool hoverClose = PtInRect(&closeBtnRect, pt);
    if (hoverClose != closeTargetVisible) {
        closeTargetVisible = hoverClose;
        SetTimer(hwnd, TIMER_CLOSE_FADE, 16, NULL);
    }

    InvalidateRect(hwnd, NULL, FALSE);
}

void OnMouseLeave(HWND hwnd) {
    barHover = false;
    reflectTargetAlpha = 0;
    trackingMouseLeave = false;
    SetTimer(hwnd, TIMER_HOVER_ANIM, 16, NULL);
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
