@echo off
echo ========================================================
echo   Building OmniConvert (C++ Material You Desktop App)
echo ========================================================

set GPP=C:\msys64\ucrt64\bin\g++.exe
if not exist "%GPP%" (
    set GPP=g++
)

echo Compiling with C++20...
"%GPP%" -std=c++20 -O2 -mwindows -municode src/main.cpp src/converter.cpp src/registry_helper.cpp -I include -I src -lole32 -luuid -lshlwapi -ldwmapi -lcomdlg32 -lshell32 -o OmniConvert.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] OmniConvert.exe built successfully!
    echo Run OmniConvert.exe or launch Launch_OmniConvert.bat
) else (
    echo.
    echo [ERROR] Build failed! Check compiler logs above.
)

pause
