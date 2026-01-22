#include "app_loader.h"

#include <shlobj.h>
#include <Shobjidl.h>
#include <atlbase.h>

std::vector<AppIcon> apps;
const int iconSize = 32;
const int spacing = 15;

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
    std::wstring searchPath = folder + L"\\*";
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &ffd);

    if (hFind == INVALID_HANDLE_VALUE) {
        MessageBox(NULL, (L"Dossier introuvable : " + folder).c_str(), L"Erreur", MB_ICONERROR);
        return;
    }

    apps.clear();

    int x = spacing;
    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring filePath = folder + L"\\" + ffd.cFileName;

            // .lnk -> résolution
            if (filePath.size() >= 4 && filePath.ends_with(L".lnk")) {
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
