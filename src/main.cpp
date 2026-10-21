#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <shellapi.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <filesystem>
#include <sstream>
#include <functional>

#include "WebView2.h"
#include "converter.hpp"
#include "registry_helper.hpp"

namespace fs = std::filesystem;

#define WM_USER_POST_WEBVIEW (WM_USER + 101)

// ---------------- COM Handler Implementations ----------------
class EnvCompletedHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    LONG m_ref = 1;
    std::function<HRESULT(HRESULT, ICoreWebView2Environment*)> m_fn;
public:
    EnvCompletedHandler(std::function<HRESULT(HRESULT, ICoreWebView2Environment*)> fn) : m_fn(std::move(fn)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Environment* env) override {
        return m_fn(res, env);
    }
};

class ControllerCompletedHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    LONG m_ref = 1;
    std::function<HRESULT(HRESULT, ICoreWebView2Controller*)> m_fn;
public:
    ControllerCompletedHandler(std::function<HRESULT(HRESULT, ICoreWebView2Controller*)> fn) : m_fn(std::move(fn)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Controller* ctl) override {
        return m_fn(res, ctl);
    }
};

class WebMessageReceivedHandler : public ICoreWebView2WebMessageReceivedEventHandler {
    LONG m_ref = 1;
    std::function<HRESULT(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*)> m_fn;
public:
    WebMessageReceivedHandler(std::function<HRESULT(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*)> fn) : m_fn(std::move(fn)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2WebMessageReceivedEventHandler) {
            *ppv = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        return m_fn(sender, args);
    }
};

class NavigationCompletedHandler : public ICoreWebView2NavigationCompletedEventHandler {
    LONG m_ref = 1;
    std::function<HRESULT(ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*)> m_fn;
public:
    NavigationCompletedHandler(std::function<HRESULT(ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*)> fn) : m_fn(std::move(fn)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2NavigationCompletedEventHandler) {
            *ppv = static_cast<ICoreWebView2NavigationCompletedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) override {
        return m_fn(sender, args);
    }
};

// ---------------- Application Globals ----------------
static HWND g_hwnd = nullptr;
static ICoreWebView2Controller* g_controller = nullptr;
static ICoreWebView2* g_webview = nullptr;
static std::wstring g_exePath;
static std::wstring g_startupFolder;
static std::wstring g_startupFile;

// Thread-safe dispatch to WebView2 via Windows message queue
static void PostMessageToFrontend(const std::string& json);

static std::string EscapeJsonString(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        if (c == '\\') ss << "\\\\";
        else if (c == '"') ss << "\\\"";
        else if (c == '\b') ss << "\\b";
        else if (c == '\f') ss << "\\f";
        else if (c == '\n') ss << "\\n";
        else if (c == '\r') ss << "\\r";
        else if (c == '\t') ss << "\\t";
        else if ((unsigned char)c < 0x20) {
            ss << "\\u00";
            const char hex[] = "0123456789abcdef";
            ss << hex[(c >> 4) & 0x0f] << hex[c & 0x0f];
        } else {
            ss << c;
        }
    }
    return ss.str();
}

// ---------------- Native OLE Drop Target ----------------
class DropTarget : public IDropTarget {
    LONG m_ref = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        if (pdwEffect) *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        if (pdwEffect) *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        if (pdwEffect) *pdwEffect = DROPEFFECT_COPY;
        if (!pDataObj) return S_OK;

        FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stg;
        if (SUCCEEDED(pDataObj->GetData(&fmt, &stg))) {
            HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
            if (hDrop) {
                UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
                std::vector<fs::path> files;
                for (UINT i = 0; i < fileCount; ++i) {
                    wchar_t filePath[MAX_PATH];
                    if (DragQueryFileW(hDrop, i, filePath, MAX_PATH) > 0) {
                        files.push_back(filePath);
                    }
                }
                GlobalUnlock(stg.hGlobal);

                if (!files.empty()) {
                    std::ostringstream json;
                    json << "{\"action\":\"filesSelected\",\"files\":[";
                    for (size_t i = 0; i < files.size(); ++i) {
                        std::string u8Path = ConverterEngine::WideToUtf8(files[i].wstring());
                        std::string u8Name = ConverterEngine::WideToUtf8(files[i].filename().wstring());
                        uintmax_t sz = 0;
                        try { if (fs::exists(files[i])) sz = fs::file_size(files[i]); } catch (...) {}
                        json << "{\"name\":\"" << EscapeJsonString(u8Name) << "\",\"path\":\"" << EscapeJsonString(u8Path) << "\",\"size\":" << sz << "}";
                        if (i + 1 < files.size()) json << ",";
                    }
                    json << "]}";
                    PostMessageToFrontend(json.str());
                }
            }
            ReleaseStgMedium(&stg);
        }
        return S_OK;
    }
};

static DropTarget* g_dropTarget = nullptr;

// ---------------- Native Windows Explorer Item Selection ----------------
static void ShowInExplorerNative(const std::wstring& rawPath) {
    if (rawPath.empty()) return;

    std::wstring normalized = rawPath;
    for (auto& ch : normalized) {
        if (ch == L'/') ch = L'\\';
    }

    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(normalized.c_str());
    if (pidl) {
        HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ILFree(pidl);
        if (SUCCEEDED(hr)) {
            return;
        }
    }

    // Fallback: If SHOpenFolderAndSelectItems failed
    if (fs::exists(normalized)) {
        std::wstring param = L"/select,\"" + normalized + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
    } else {
        fs::path p(normalized);
        if (p.has_parent_path() && fs::exists(p.parent_path())) {
            ShellExecuteW(nullptr, L"open", p.parent_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
}

typedef HRESULT(STDAPICALLTYPE* CreateCoreWebView2EnvironmentWithOptionsFunc)(
    PCWSTR browserExecutableFolder,
    PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions* environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environmentCreatedHandler);

// Thread-safe dispatch to WebView2 via Windows message queue
static void PostMessageToFrontend(const std::string& json) {
    if (!g_hwnd) return;
    std::string* pCopy = new std::string(json);
    PostMessageW(g_hwnd, WM_USER_POST_WEBVIEW, 0, reinterpret_cast<LPARAM>(pCopy));
}

// ---------------- Native File Dialog Helpers ----------------
static void OpenNativeFilesDialog() {
    std::vector<wchar_t> buffer(65536, 0);
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = static_cast<DWORD>(buffer.size());
    ofn.lpstrFilter = L"All Supported Media & Docs\0*.mp4;*.mkv;*.avi;*.mov;*.webm;*.wmv;*.flv;*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.ico;*.tiff;*.gif;*.pdf;*.docx;*.pptx;*.txt;*.html;*.md\0PDF Documents (*.pdf)\0*.pdf\0Video Files\0*.mp4;*.mkv;*.avi;*.mov;*.webm;*.wmv;*.flv\0Images & Photos\0*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.ico;*.tiff;*.gif\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn)) {
        std::vector<fs::path> files;
        wchar_t* p = buffer.data();
        std::wstring first(p);
        p += first.length() + 1;

        if (*p == 0) {
            files.push_back(fs::path(first));
        } else {
            fs::path dir(first);
            while (*p != 0) {
                std::wstring fname(p);
                files.push_back(dir / fname);
                p += fname.length() + 1;
            }
        }

        std::ostringstream json;
        json << "{\"action\":\"filesSelected\",\"files\":[";
        for (size_t i = 0; i < files.size(); ++i) {
            std::string u8Path = ConverterEngine::WideToUtf8(files[i].wstring());
            std::string u8Name = ConverterEngine::WideToUtf8(files[i].filename().wstring());
            uintmax_t sz = 0;
            try { if (fs::exists(files[i])) sz = fs::file_size(files[i]); } catch (...) {}
            json << "{\"name\":\"" << EscapeJsonString(u8Name) << "\",\"path\":\"" << EscapeJsonString(u8Path) << "\",\"size\":" << sz << "}";
            if (i + 1 < files.size()) json << ",";
        }
        json << "]}";

        PostMessageToFrontend(json.str());
    }
}

static void OpenNativeFolderDialog() {
    IFileDialog* pfd = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
        DWORD dwOptions;
        if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
            pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }
        if (SUCCEEDED(pfd->Show(g_hwnd))) {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    fs::path folderPath(pszPath);
                    CoTaskMemFree(pszPath);

                    std::vector<fs::path> files;
                    try {
                        for (const auto& entry : fs::directory_iterator(folderPath)) {
                            if (entry.is_regular_file()) {
                                files.push_back(entry.path());
                            }
                        }
                    } catch (...) {}

                    std::ostringstream json;
                    json << "{\"action\":\"filesSelected\",\"files\":[";
                    for (size_t i = 0; i < files.size(); ++i) {
                        std::string u8Path = ConverterEngine::WideToUtf8(files[i].wstring());
                        std::string u8Name = ConverterEngine::WideToUtf8(files[i].filename().wstring());
                        uintmax_t sz = 0;
                        try { sz = fs::file_size(files[i]); } catch (...) {}
                        json << "{\"name\":\"" << EscapeJsonString(u8Name) << "\",\"path\":\"" << EscapeJsonString(u8Path) << "\",\"size\":" << sz << "}";
                        if (i + 1 < files.size()) json << ",";
                    }
                    json << "]}";

                    PostMessageToFrontend(json.str());
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
}

// ---------------- Process Incoming Web Messages ----------------
static void HandleWebMessage(std::string raw) {
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        std::string unescaped;
        for (size_t i = 1; i < raw.size() - 1; ++i) {
            if (raw[i] == '\\' && i + 1 < raw.size() - 1) {
                char next = raw[++i];
                if (next == '"') unescaped += '"';
                else if (next == '\\') unescaped += '\\';
                else if (next == 'n') unescaped += '\n';
                else if (next == 'r') unescaped += '\r';
                else if (next == 't') unescaped += '\t';
                else unescaped += next;
            } else {
                unescaped += raw[i];
            }
        }
        raw = unescaped;
    }

    auto findField = [&](const std::string& key) -> std::string {
        std::string pattern = "\"" + key + "\":\"";
        size_t pos = raw.find(pattern);
        if (pos != std::string::npos) {
            size_t start = pos + pattern.length();
            size_t end = raw.find("\"", start);
            if (end != std::string::npos) return raw.substr(start, end - start);
        }
        return "";
    };

    auto findInt = [&](const std::string& key, int defaultVal) -> int {
        std::string pattern = "\"" + key + "\":";
        size_t pos = raw.find(pattern);
        if (pos != std::string::npos) {
            size_t start = pos + pattern.length();
            while (start < raw.size() && (raw[start] == ' ' || raw[start] == '"')) start++;
            size_t end = start;
            while (end < raw.size() && isdigit((unsigned char)raw[end])) end++;
            if (end > start) {
                try { return std::stoi(raw.substr(start, end - start)); } catch (...) {}
            }
        }
        return defaultVal;
    };

    auto findBool = [&](const std::string& key) -> bool {
        std::string pattern = "\"" + key + "\":";
        size_t pos = raw.find(pattern);
        if (pos != std::string::npos) {
            size_t start = pos + pattern.length();
            while (start < raw.size() && raw[start] == ' ') start++;
            if (raw.substr(start, 4) == "true") return true;
        }
        return false;
    };

    std::string action = findField("action");

    if (action == "browseFiles") {
        OpenNativeFilesDialog();
    } else if (action == "browseFolder") {
        OpenNativeFolderDialog();
    } else if (action == "cancelConvert") {
        ConverterEngine::CancelCurrentConversion();
        PostMessageToFrontend("{\"action\":\"conversionCancelled\"}");
    } else if (action == "openFile") {
        std::string path = findField("path");
        if (!path.empty()) {
            std::wstring wpath = ConverterEngine::Utf8ToWide(path);
            for (auto& ch : wpath) {
                if (ch == L'/') ch = L'\\';
            }
            ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    } else if (action == "showInExplorer") {
        std::string path = findField("path");
        if (!path.empty()) {
            std::wstring wpath = ConverterEngine::Utf8ToWide(path);
            ShowInExplorerNative(wpath);
        }
    } else if (action == "toggleStartup") {
        bool enable = findBool("enabled");
        RegistryHelper::SetStartupEnabled(enable, g_exePath);
    } else if (action == "toggleContextMenu") {
        bool enable = findBool("enabled");
        RegistryHelper::SetContextMenuEnabled(enable, g_exePath);
    } else if (action == "getInitialState") {
        bool startup = RegistryHelper::IsStartupEnabled();
        bool contextMenu = RegistryHelper::IsContextMenuEnabled();

        std::ostringstream json;
        json << "{\"action\":\"initialState\",\"settings\":{"
             << "\"startup\":" << (startup ? "true" : "false") << ","
             << "\"contextMenu\":" << (contextMenu ? "true" : "false")
             << "},\"startupFiles\":[";

        if (!g_startupFile.empty() && fs::exists(g_startupFile)) {
            std::string u8Path = ConverterEngine::WideToUtf8(g_startupFile);
            std::string u8Name = ConverterEngine::WideToUtf8(fs::path(g_startupFile).filename().wstring());
            uintmax_t sz = 0;
            try { sz = fs::file_size(g_startupFile); } catch (...) {}
            json << "{\"name\":\"" << EscapeJsonString(u8Name) << "\",\"path\":\"" << EscapeJsonString(u8Path) << "\",\"size\":" << sz << "}";
        } else if (!g_startupFolder.empty() && fs::is_directory(g_startupFolder)) {
            bool first = true;
            for (const auto& entry : fs::directory_iterator(g_startupFolder)) {
                if (entry.is_regular_file()) {
                    if (!first) json << ",";
                    first = false;
                    std::string u8Path = ConverterEngine::WideToUtf8(entry.path().wstring());
                    std::string u8Name = ConverterEngine::WideToUtf8(entry.path().filename().wstring());
                    uintmax_t sz = 0;
                    try { sz = fs::file_size(entry.path()); } catch (...) {}
                    json << "{\"name\":\"" << EscapeJsonString(u8Name) << "\",\"path\":\"" << EscapeJsonString(u8Path) << "\",\"size\":" << sz << "}";
                }
            }
        }
        json << "]}";
        PostMessageToFrontend(json.str());
    } else if (action == "startConvert") {
        std::string targetFormat = findField("targetFormat");
        std::string category = findField("category");
        std::string quality = findField("quality");
        if (quality.empty()) quality = "high";
        bool extractAudio = findBool("extractAudio");
        std::string destination = findField("destination");
        if (destination.empty()) destination = "same";

        bool isCompressMode = findBool("isCompressMode");
        int pdfQuality = findInt("pdfQuality", 75);
        int pdfPpi = findInt("pdfPpi", 150);
        int videoQualityPct = findInt("videoQualityPct", 65);
        std::string videoResolution = findField("videoResolution");
        if (videoResolution.empty()) videoResolution = "original";

        // Find file paths from raw JSON
        std::vector<std::wstring> filePaths;
        size_t ppos = 0;
        std::string ptag = "\"path\":\"";
        while ((ppos = raw.find(ptag, ppos)) != std::string::npos) {
            size_t start = ppos + ptag.length();
            size_t end = raw.find("\"", start);
            if (end != std::string::npos) {
                std::string pstr = raw.substr(start, end - start);
                std::string clean;
                for (size_t i = 0; i < pstr.length(); ++i) {
                    if (pstr[i] == '\\' && i + 1 < pstr.length()) {
                        clean += pstr[++i];
                    } else {
                        clean += pstr[i];
                    }
                }
                filePaths.push_back(ConverterEngine::Utf8ToWide(clean));
                ppos = end + 1;
            } else break;
        }

        if (filePaths.empty()) {
            PostMessageToFrontend("{\"action\":\"conversionError\",\"error\":\"No valid file paths received for conversion.\"}");
            return;
        }

        std::thread([filePaths, targetFormat, category, quality, extractAudio, destination,
                     isCompressMode, pdfQuality, pdfPpi, videoQualityPct, videoResolution]() {
            auto startTime = std::chrono::steady_clock::now();
            std::wstring lastResult;

            for (size_t i = 0; i < filePaths.size(); ++i) {
                ConverterEngine::ConvertOptions opt;
                opt.inputPath = filePaths[i];
                opt.targetExt = ConverterEngine::Utf8ToWide(targetFormat);
                opt.category = category;
                opt.qualityPreset = quality;
                opt.extractAudio = extractAudio;
                opt.destinationChoice = destination;

                opt.isCompressMode = isCompressMode;
                opt.pdfQuality = pdfQuality;
                opt.pdfPpi = pdfPpi;
                opt.videoQualityPct = videoQualityPct;
                opt.videoResolution = videoResolution;

                std::wstring resultPath;
                std::string errorMsg;

                bool ok = ConverterEngine::ExecuteConversion(
                    opt,
                    [i, total = filePaths.size()](int pct, const std::string& detail) {
                        int overall = static_cast<int>(((float)i / total) * 100.0f + ((float)pct / total));
                        std::ostringstream json;
                        json << "{\"action\":\"conversionProgress\",\"percent\":" << overall << ",\"detail\":\"" << EscapeJsonString(detail) << "\"}";
                        PostMessageToFrontend(json.str());
                    },
                    resultPath,
                    errorMsg
                );

                if (!ok) {
                    if (errorMsg == "Conversion cancelled by user.") {
                        PostMessageToFrontend("{\"action\":\"conversionCancelled\"}");
                    } else {
                        std::ostringstream json;
                        json << "{\"action\":\"conversionError\",\"error\":\"" << EscapeJsonString(errorMsg) << "\"}";
                        PostMessageToFrontend(json.str());
                    }
                    return;
                }
                lastResult = resultPath;
            }

            auto endTime = std::chrono::steady_clock::now();
            auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            float secs = durationMs / 1000.0f;
            char durBuf[32];
            snprintf(durBuf, sizeof(durBuf), "%.1fs", secs);

            uintmax_t outSize = 0;
            try { if (fs::exists(lastResult)) outSize = fs::file_size(lastResult); } catch (...) {}
            char szBuf[32];
            if (outSize > 1024 * 1024) {
                snprintf(szBuf, sizeof(szBuf), "%.1f MB", outSize / (1024.0f * 1024.0f));
            } else {
                snprintf(szBuf, sizeof(szBuf), "%.1f KB", outSize / 1024.0f);
            }

            std::string u8Output = ConverterEngine::WideToUtf8(lastResult);
            std::ostringstream json;
            json << "{\"action\":\"conversionComplete\","
                 << "\"outputFile\":\"" << EscapeJsonString(u8Output) << "\","
                 << "\"duration\":\"" << durBuf << "\","
                 << "\"sizeFormatted\":\"" << szBuf << "\"}";
            PostMessageToFrontend(json.str());
        }).detach();
    }
}

// ---------------- Window Procedure ----------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        if (g_controller) {
            RECT bounds;
            GetClientRect(hwnd, &bounds);
            g_controller->put_Bounds(bounds);
        }
        return 0;
    }
    case WM_GETMINMAXINFO: {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 840;
        lpMMI->ptMinTrackSize.y = 560;
        return 0;
    }
    case WM_USER_POST_WEBVIEW: {
        std::string* pJson = reinterpret_cast<std::string*>(lParam);
        if (pJson) {
            if (g_webview) {
                std::wstring wjson = ConverterEngine::Utf8ToWide(*pJson);
                g_webview->PostWebMessageAsJson(wjson.c_str());
            }
            delete pJson;
        }
        return 0;
    }
    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        std::ostringstream json;
        json << "{\"action\":\"filesSelected\",\"files\":[";
        for (UINT i = 0; i < count; ++i) {
            wchar_t filePath[MAX_PATH];
            if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
                std::wstring wPath(filePath);
                fs::path p(wPath);
                std::string u8Path = ConverterEngine::WideToUtf8(wPath);
                std::string u8Name = ConverterEngine::WideToUtf8(p.filename().wstring());
                uintmax_t sz = 0;
                try { if (fs::exists(p)) sz = fs::file_size(p); } catch (...) {}
                if (i > 0) json << ",";
                json << "{\"name\":\"" << EscapeJsonString(u8Name) << "\",\"path\":\"" << EscapeJsonString(u8Path) << "\",\"size\":" << sz << "}";
            }
        }
        json << "]}";
        DragFinish(hDrop);
        PostMessageToFrontend(json.str());
        return 0;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
    OleInitialize(nullptr);

    wchar_t exeBuffer[MAX_PATH];
    GetModuleFileNameW(nullptr, exeBuffer, MAX_PATH);
    g_exePath = exeBuffer;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(pCmdLine, &argc);
    if (argv && argc > 0) {
        for (int i = 0; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--convert-folder" && i + 1 < argc) {
                g_startupFolder = argv[++i];
            } else if (arg == L"--convert-file" && i + 1 < argc) {
                g_startupFile = argv[++i];
            } else if (arg == L"--register-context-menu") {
                RegistryHelper::SetContextMenuEnabled(true, g_exePath);
                LocalFree(argv);
                return 0;
            } else if (arg == L"--unregister-context-menu") {
                RegistryHelper::SetContextMenuEnabled(false, g_exePath);
                LocalFree(argv);
                return 0;
            } else if (arg == L"--register-startup") {
                RegistryHelper::SetStartupEnabled(true, g_exePath);
                LocalFree(argv);
                return 0;
            } else if (arg == L"--unregister-startup") {
                RegistryHelper::SetStartupEnabled(false, g_exePath);
                LocalFree(argv);
                return 0;
            }
        }
        LocalFree(argv);
    }

    const wchar_t CLASS_NAME[] = L"OmniConvertWindowClass";
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(15, 17, 21));
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    int width = 1060;
    int height = 720;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - width) / 2;
    int posY = (screenH - height) / 2;

    g_hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"OmniConvert — Universal Offline Converter",
        WS_OVERLAPPEDWINDOW,
        posX, posY, width, height,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hwnd) return 1;

    g_dropTarget = new DropTarget();
    RegisterDragDrop(g_hwnd, g_dropTarget);
    DragAcceptFiles(g_hwnd, TRUE);

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(g_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    fs::path exeDir = fs::path(g_exePath).parent_path();
    fs::path loaderDll = exeDir / L"WebView2Loader.dll";

    HMODULE hLoader = LoadLibraryW(loaderDll.c_str());
    if (!hLoader) hLoader = LoadLibraryW(L"WebView2Loader.dll");
    if (!hLoader) {
        MessageBoxW(g_hwnd, L"Could not locate WebView2Loader.dll.", L"OmniConvert Error", MB_ICONERROR);
        return 1;
    }

    auto CreateEnvironmentWithOptions = (CreateCoreWebView2EnvironmentWithOptionsFunc)
        GetProcAddress(hLoader, "CreateCoreWebView2EnvironmentWithOptions");

    if (!CreateEnvironmentWithOptions) {
        MessageBoxW(g_hwnd, L"Could not find CreateCoreWebView2EnvironmentWithOptions entry point.", L"OmniConvert Error", MB_ICONERROR);
        return 1;
    }

    PWSTR localAppData = nullptr;
    std::wstring userDataFolder;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        userDataFolder = std::wstring(localAppData) + L"\\OmniConvert\\WebViewData";
        CoTaskMemFree(localAppData);
    }

    CreateEnvironmentWithOptions(
        nullptr,
        userDataFolder.empty() ? nullptr : userDataFolder.c_str(),
        nullptr,
        new EnvCompletedHandler([exeDir](HRESULT res, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(res) || !env) return res;

            env->CreateCoreWebView2Controller(
                g_hwnd,
                new ControllerCompletedHandler([exeDir](HRESULT ctlRes, ICoreWebView2Controller* ctl) -> HRESULT {
                    if (FAILED(ctlRes) || !ctl) return ctlRes;
                    g_controller = ctl;
                    ctl->AddRef();

                    RECT bounds;
                    GetClientRect(g_hwnd, &bounds);
                    ctl->put_Bounds(bounds);

                    ctl->get_CoreWebView2(&g_webview);
                    if (!g_webview) return E_FAIL;
                    g_webview->AddRef();

                    EventRegistrationToken token;
                    g_webview->add_WebMessageReceived(
                        new WebMessageReceivedHandler([](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            LPWSTR rawStr = nullptr;
                            if (SUCCEEDED(args->TryGetWebMessageAsString(&rawStr)) && rawStr) {
                                std::string u8Msg = ConverterEngine::WideToUtf8(rawStr);
                                CoTaskMemFree(rawStr);
                                HandleWebMessage(u8Msg);
                                return S_OK;
                            }
                            LPWSTR rawJson = nullptr;
                            if (SUCCEEDED(args->get_WebMessageAsJson(&rawJson)) && rawJson) {
                                std::string u8Msg = ConverterEngine::WideToUtf8(rawJson);
                                CoTaskMemFree(rawJson);
                                HandleWebMessage(u8Msg);
                                return S_OK;
                            }
                            return S_OK;
                        }),
                        &token
                    );

                    fs::path htmlPath = exeDir / L"ui" / L"index.html";
                    std::wstring url = L"file:///" + htmlPath.wstring();
                    for (auto& ch : url) {
                        if (ch == L'\\') ch = L'/';
                    }
                    g_webview->Navigate(url.c_str());

                    return S_OK;
                })
            );
            return S_OK;
        })
    );

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_webview) { g_webview->Release(); g_webview = nullptr; }
    if (g_controller) { g_controller->Release(); g_controller = nullptr; }
    if (g_dropTarget) {
        RevokeDragDrop(g_hwnd);
        g_dropTarget->Release();
        g_dropTarget = nullptr;
    }
    if (hLoader) FreeLibrary(hLoader);

    OleUninitialize();
    return (int)msg.wParam;
}
