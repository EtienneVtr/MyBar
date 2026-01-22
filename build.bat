@echo off
cl main.cpp window.cpp ui.cpp app_loader.cpp ^
  user32.lib gdi32.lib shell32.lib dwmapi.lib ole32.lib msimg32.lib ^
  /O2 /std:c++20 /EHsc /DUNICODE /D_UNICODE /Fe:mybar.exe
