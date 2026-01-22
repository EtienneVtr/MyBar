#pragma once
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <vector>
#include <string>

struct AppIcon {
    RECT rect;
    std::wstring path;
    HICON icon;
};

extern std::vector<AppIcon> apps;
extern const int iconSize;
extern const int spacing;

std::wstring ResolveShortcut(const std::wstring& lnkPath);
void LoadAppsFromFolder(const std::wstring& folder);
