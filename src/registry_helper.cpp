#include "registry_helper.hpp"
#include <shlwapi.h>

namespace RegistryHelper {

    static const wchar_t* RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static const wchar_t* APP_VALUE_NAME = L"OmniConvert";

    bool IsStartupEnabled() {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD type = 0;
            DWORD size = 0;
            LONG res = RegQueryValueExW(hKey, APP_VALUE_NAME, nullptr, &type, nullptr, &size);
            RegCloseKey(hKey);
            return (res == ERROR_SUCCESS);
        }
        return false;
    }

    bool SetStartupEnabled(bool enable, const std::wstring& exePath) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            if (enable) {
                std::wstring cmd = L"\"" + exePath + L"\"";
                LONG res = RegSetValueExW(hKey, APP_VALUE_NAME, 0, REG_SZ,
                    reinterpret_cast<const BYTE*>(cmd.c_str()),
                    static_cast<DWORD>((cmd.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hKey);
                return (res == ERROR_SUCCESS);
            } else {
                RegDeleteValueW(hKey, APP_VALUE_NAME);
                RegCloseKey(hKey);
                return true;
            }
        }
        return false;
    }

    static bool SetSubkeyString(HKEY hRoot, const std::wstring& subkey, const wchar_t* valueName, const std::wstring& data) {
        HKEY hKey;
        if (RegCreateKeyExW(hRoot, subkey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            LONG res = RegSetValueExW(hKey, valueName, 0, REG_SZ,
                reinterpret_cast<const BYTE*>(data.c_str()),
                static_cast<DWORD>((data.length() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
            return (res == ERROR_SUCCESS);
        }
        return false;
    }

    static void DeleteSubkeyTree(HKEY hRoot, const std::wstring& subkey) {
        SHDeleteKeyW(hRoot, subkey.c_str());
    }

    bool IsContextMenuEnabled() {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\OmniConvert", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }
        return false;
    }

    bool SetContextMenuEnabled(bool enable, const std::wstring& exePath) {
        const std::wstring dirKey = L"Software\\Classes\\Directory\\shell\\OmniConvert";
        const std::wstring dirBgKey = L"Software\\Classes\\Directory\\Background\\shell\\OmniConvert";
        const std::wstring fileKey = L"Software\\Classes\\*\\shell\\OmniConvert";

        if (enable) {
            std::wstring quotedExe = L"\"" + exePath + L"\"";

            // 1. Right click on Folder
            SetSubkeyString(HKEY_CURRENT_USER, dirKey, nullptr, L"Convert with OmniConvert");
            SetSubkeyString(HKEY_CURRENT_USER, dirKey, L"Icon", quotedExe);
            SetSubkeyString(HKEY_CURRENT_USER, dirKey + L"\\command", nullptr, quotedExe + L" --convert-folder \"%1\"");

            // 2. Right click inside Folder background
            SetSubkeyString(HKEY_CURRENT_USER, dirBgKey, nullptr, L"Convert files here with OmniConvert");
            SetSubkeyString(HKEY_CURRENT_USER, dirBgKey, L"Icon", quotedExe);
            SetSubkeyString(HKEY_CURRENT_USER, dirBgKey + L"\\command", nullptr, quotedExe + L" --convert-folder \"%V\"");

            // 3. Right click on any File
            SetSubkeyString(HKEY_CURRENT_USER, fileKey, nullptr, L"Convert with OmniConvert");
            SetSubkeyString(HKEY_CURRENT_USER, fileKey, L"Icon", quotedExe);
            SetSubkeyString(HKEY_CURRENT_USER, fileKey + L"\\command", nullptr, quotedExe + L" --convert-file \"%1\"");

            return true;
        } else {
            DeleteSubkeyTree(HKEY_CURRENT_USER, dirKey);
            DeleteSubkeyTree(HKEY_CURRENT_USER, dirBgKey);
            DeleteSubkeyTree(HKEY_CURRENT_USER, fileKey);
            return true;
        }
    }
}
