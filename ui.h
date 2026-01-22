#pragma once
#define UNICODE
#define _UNICODE

#include <windows.h>

#define TIMER_CLOSE_FADE 1
#define TIMER_HOVER_ANIM 2

extern RECT closeBtnRect;
extern int closeAlpha;
extern bool closeTargetVisible;

extern bool barHover;
extern int mouseX;
extern int mouseY;
extern int reflectAlpha;
extern int reflectTargetAlpha;

void DrawUI(HWND hwnd, HDC hdc);
void OnLeftButtonDown(HWND hwnd, int x, int y);
void OnMouseMove(HWND hwnd, int x, int y);
void OnMouseLeave(HWND hwnd);
void OnTimer(HWND hwnd, WPARAM wParam);
