#pragma once
#include <windows.h>
#include <string>

namespace RegistryHelper {
    // Windows Startup (HKCU\Software\Microsoft\Windows\CurrentVersion\Run)
    bool IsStartupEnabled();
    bool SetStartupEnabled(bool enable, const std::wstring& exePath);

    // Explorer Right-Click Context Menu (HKCU\Software\Classes\...)
    bool IsContextMenuEnabled();
    bool SetContextMenuEnabled(bool enable, const std::wstring& exePath);
}
