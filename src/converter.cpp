#include "converter.hpp"
#include <windows.h>
#include <shlobj.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <chrono>
#include <atomic>

namespace fs = std::filesystem;

namespace ConverterEngine {

    static std::atomic<HANDLE> g_hProcess{ nullptr };
    static std::atomic<bool> g_cancelRequested{ false };

    void CancelCurrentConversion() {
        g_cancelRequested = true;
        HANDLE h = g_hProcess.load();
        if (h && h != INVALID_HANDLE_VALUE) {
            TerminateProcess(h, 1);
        }
    }

    std::string WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string res(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &res[0], size, nullptr, nullptr);
        return res;
    }

    std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
        std::wstring res(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &res[0], size);
        return res;
    }

    std::wstring GetExecutableDir() {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        fs::path p(path);
        return p.parent_path().wstring();
    }

    std::wstring FindFFmpegPath() {
        std::wstring exeDir = GetExecutableDir();
        std::wstring localFfmpeg = exeDir + L"\\ffmpeg.exe";
        if (fs::exists(localFfmpeg)) return L"\"" + localFfmpeg + L"\"";

        wchar_t localApp[MAX_PATH];
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
            std::wstring wingetDir = std::wstring(localApp) + L"\\Microsoft\\WinGet\\Packages";
            try {
                if (fs::exists(wingetDir)) {
                    for (const auto& entry : fs::recursive_directory_iterator(wingetDir)) {
                        if (entry.path().filename() == L"ffmpeg.exe") {
                            return L"\"" + entry.path().wstring() + L"\"";
                        }
                    }
                }
            } catch (...) {}
        }

        wchar_t found[MAX_PATH];
        if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, found, nullptr) > 0) {
            return L"\"" + std::wstring(found) + L"\"";
        }

        return L"ffmpeg";
    }

    std::wstring FindFFprobePath() {
        std::wstring ffmpeg = FindFFmpegPath();
        if (ffmpeg.length() >= 12) {
            // strip quotes if any
            std::wstring clean = ffmpeg;
            if (clean.front() == L'"') clean = clean.substr(1, clean.length() - 2);
            fs::path p(clean);
            fs::path probe = p.parent_path() / L"ffprobe.exe";
            if (fs::exists(probe)) return L"\"" + probe.wstring() + L"\"";
        }

        wchar_t found[MAX_PATH];
        if (SearchPathW(nullptr, L"ffprobe.exe", nullptr, MAX_PATH, found, nullptr) > 0) {
            return L"\"" + std::wstring(found) + L"\"";
        }
        return L"ffprobe";
    }

    static std::string FormatSeconds(double totalSecs) {
        if (totalSecs < 0) totalSecs = 0;
        int hours = static_cast<int>(totalSecs) / 3600;
        int mins = (static_cast<int>(totalSecs) % 3600) / 60;
        int secs = static_cast<int>(totalSecs) % 60;
        char buf[32];
        if (hours > 0) {
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, mins, secs);
        } else {
            snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
        }
        return buf;
    }

    double GetMediaDurationSeconds(const std::wstring& mediaPath) {
        std::wstring ffprobe = FindFFprobePath();
        std::wstringstream cmd;
        cmd << ffprobe << L" -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"" << mediaPath << L"\"";

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE hRead = nullptr, hWrite = nullptr;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return 0.0;
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        std::wstring cmdStr = cmd.str();
        std::vector<wchar_t> cmdBuf(cmdStr.begin(), cmdStr.end());
        cmdBuf.push_back(0);

        double duration = 0.0;
        if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(hWrite);
            char buf[128];
            DWORD readBytes = 0;
            std::string acc;
            while (ReadFile(hRead, buf, sizeof(buf) - 1, &readBytes, nullptr) && readBytes > 0) {
                buf[readBytes] = 0;
                acc += buf;
            }
            CloseHandle(hRead);
            WaitForSingleObject(pi.hProcess, 3000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            try {
                duration = std::stod(acc);
            } catch (...) {
                duration = 0.0;
            }
        } else {
            CloseHandle(hRead);
            CloseHandle(hWrite);
        }
        return duration;
    }

    std::wstring DetermineOutputPath(const std::wstring& inputPath, const std::wstring& targetExt, const std::string& destinationChoice, bool isCompress) {
        fs::path inPath(inputPath);
        fs::path destDir;

        if (destinationChoice == "downloads") {
            PWSTR knownPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &knownPath))) {
                destDir = fs::path(knownPath);
                CoTaskMemFree(knownPath);
            } else {
                destDir = inPath.parent_path();
            }
        } else if (destinationChoice == "desktop") {
            PWSTR knownPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &knownPath))) {
                destDir = fs::path(knownPath);
                CoTaskMemFree(knownPath);
            } else {
                destDir = inPath.parent_path();
            }
        } else {
            destDir = inPath.parent_path();
        }

        std::wstring baseStem = inPath.stem().wstring();
        std::wstring cleanExt = targetExt;
        if (!cleanExt.empty() && cleanExt[0] == L'.') cleanExt = cleanExt.substr(1);

        std::wstring suffix = isCompress ? L"_compressed" : L"_converted";
        fs::path candidate = destDir / (baseStem + suffix + L"." + cleanExt);

        int counter = 1;
        while (fs::exists(candidate)) {
            candidate = destDir / (baseStem + suffix + L"(" + std::to_wstring(counter) + L")." + cleanExt);
            counter++;
        }

        return candidate.wstring();
    }

    // Executes process with non-blocking line-by-line pipes for REAL live progress
    static bool RunProcessWithRealProgress(
        const std::wstring& commandLine,
        ProgressCallback onProgress,
        std::string& outError,
        double totalDurationSeconds = 0.0)
    {
        g_cancelRequested = false;

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            outError = "Failed to create I/O pipes. Error code: " + std::to_string(GetLastError());
            return false;
        }
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        si.wShowWindow = SW_HIDE;
        ZeroMemory(&pi, sizeof(pi));

        std::vector<wchar_t> cmdBuf(commandLine.begin(), commandLine.end());
        cmdBuf.push_back(0);

        BOOL success = CreateProcessW(
            nullptr,
            cmdBuf.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        // Close write pipe in parent process so EOF occurs when child terminates
        CloseHandle(hWritePipe);

        if (!success) {
            CloseHandle(hReadPipe);
            outError = "Could not spawn process. Error code: " + std::to_string(GetLastError());
            return false;
        }

        g_hProcess.store(pi.hProcess);

        char buf[2048];
        DWORD bytesRead = 0;
        std::string lineAcc;
        std::string lastErrorMsg;

        // Progress state for FFmpeg
        double currentOutTimeSecs = 0.0;
        std::string currentSpeed = "";
        std::string currentFps = "";

        while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            if (g_cancelRequested) {
                TerminateProcess(pi.hProcess, 1);
                break;
            }

            buf[bytesRead] = 0;
            lineAcc.append(buf, bytesRead);

            size_t newlinePos;
            while ((newlinePos = lineAcc.find('\n')) != std::string::npos) {
                std::string line = lineAcc.substr(0, newlinePos);
                lineAcc.erase(0, newlinePos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;

                lastErrorMsg = line;

                // 1. Check for Python PROGRESS:<percent>:<detail>
                if (line.rfind("PROGRESS:", 0) == 0) {
                    size_t firstColon = line.find(':', 9);
                    if (firstColon != std::string::npos) {
                        std::string pctStr = line.substr(9, firstColon - 9);
                        std::string msg = line.substr(firstColon + 1);
                        try {
                            int pct = std::stoi(pctStr);
                            if (pct < 0) pct = 0;
                            if (pct > 100) pct = 100;
                            if (onProgress) onProgress(pct, msg);
                        } catch (...) {}
                    }
                }
                // 2. Check for FFmpeg key=value progress outputs
                else {
                    size_t eqPos = line.find('=');
                    if (eqPos != std::string::npos) {
                        std::string key = line.substr(0, eqPos);
                        std::string val = line.substr(eqPos + 1);

                        if (key == "out_time_us") {
                            try {
                                long long us = std::stoll(val);
                                currentOutTimeSecs = us / 1000000.0;
                            } catch (...) {}
                        } else if (key == "speed") {
                            currentSpeed = val;
                        } else if (key == "fps") {
                            currentFps = val;
                        } else if (key == "progress") {
                            if (val == "end") {
                                if (onProgress) onProgress(100, "Encoding finalized successfully!");
                            } else if (totalDurationSeconds > 0.0) {
                                int pct = static_cast<int>((currentOutTimeSecs / totalDurationSeconds) * 100.0);
                                if (pct < 1) pct = 1;
                                if (pct > 99) pct = 99; // keep at 99 until finished

                                std::ostringstream msg;
                                msg << "Encoding: " << pct << "% • "
                                    << FormatSeconds(currentOutTimeSecs) << " / " << FormatSeconds(totalDurationSeconds);
                                if (!currentSpeed.empty() && currentSpeed != "N/A") {
                                    msg << " • " << currentSpeed;
                                }
                                if (onProgress) onProgress(pct, msg.str());
                            } else {
                                // Duration unknown: display running timestamp and speed
                                std::ostringstream msg;
                                msg << "Processed: " << FormatSeconds(currentOutTimeSecs);
                                if (!currentSpeed.empty()) msg << " • " << currentSpeed;
                                if (onProgress) onProgress(50, msg.str());
                            }
                        }
                    }
                }
            }
        }

        CloseHandle(hReadPipe);

        DWORD exitCode = 1;
        WaitForSingleObject(pi.hProcess, 3000);
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        g_hProcess.store(nullptr);

        if (g_cancelRequested) {
            outError = "Conversion cancelled by user.";
            return false;
        }

        if (exitCode != 0) {
            outError = "Process error (" + std::to_string(exitCode) + "): " + lastErrorMsg;
            return false;
        }

        if (onProgress) onProgress(100, "Operation complete!");
        return true;
    }

    bool ExecuteConversion(
        const ConvertOptions& options,
        ProgressCallback onProgress,
        std::wstring& outResultPath,
        std::string& outErrorMessage)
    {
        if (!fs::exists(options.inputPath)) {
            outErrorMessage = "File not found: " + WideToUtf8(options.inputPath) + ". Please use the 'Browse Files' button to select files.";
            return false;
        }

        outResultPath = DetermineOutputPath(options.inputPath, options.targetExt, options.destinationChoice, options.isCompressMode);

        std::wstring inExt = fs::path(options.inputPath).extension().wstring();
        if (!inExt.empty() && inExt[0] == L'.') inExt = inExt.substr(1);
        std::wstring outExt = options.targetExt;
        if (!outExt.empty() && outExt[0] == L'.') outExt = outExt.substr(1);

        if (onProgress) onProgress(1, "Analyzing file and preparing media pipelines...");

        std::wstring exeDir = GetExecutableDir();
        std::wstring scriptPath = exeDir + L"\\src\\doc_converter.py";
        if (!fs::exists(scriptPath)) {
            scriptPath = exeDir + L"\\doc_converter.py";
        }

        // 1. PDF Compression Mode (Real Live Page-by-Page Progress)
        if (options.isCompressMode && inExt == L"pdf") {
            std::wstringstream cmd;
            cmd << L"python -X utf8 -u \"" << scriptPath << L"\" --compress --input \"" << options.inputPath
                << L"\" --output \"" << outResultPath << L"\" --quality " << options.pdfQuality
                << L" --ppi " << options.pdfPpi;
            return RunProcessWithRealProgress(cmd.str(), onProgress, outErrorMessage, 0.0);
        }

        // 2. Video Compression Mode (Real Live Second-by-Second Progress)
        if (options.isCompressMode && (options.category == "video" || inExt == L"mp4" || inExt == L"mkv" || inExt == L"avi" || inExt == L"mov")) {
            double duration = GetMediaDurationSeconds(options.inputPath);
            std::wstring ffmpeg = FindFFmpegPath();

            int crf = 18 + static_cast<int>((100 - options.videoQualityPct) * 0.22);
            if (crf < 18) crf = 18;
            if (crf > 38) crf = 38;

            std::wstringstream cmd;
            cmd << ffmpeg << L" -y -hide_banner -loglevel error -progress pipe:1 -i \"" << options.inputPath << L"\" ";

            if (options.videoResolution == "1080p") {
                cmd << L"-vf \"scale=-2:1080\" ";
            } else if (options.videoResolution == "720p") {
                cmd << L"-vf \"scale=-2:720\" ";
            } else if (options.videoResolution == "480p") {
                cmd << L"-vf \"scale=-2:480\" ";
            }

            cmd << L"-c:v libx264 -crf " << crf << L" -preset medium -c:a aac -b:a 128k \"" << outResultPath << L"\"";
            return RunProcessWithRealProgress(cmd.str(), onProgress, outErrorMessage, duration);
        }

        // 3. Document Format Conversions
        bool isDoc = (options.category == "doc") ||
            (inExt == L"docx" || inExt == L"doc" || inExt == L"pdf" || inExt == L"pptx" || inExt == L"ppt" || inExt == L"txt" || inExt == L"md");

        if (isDoc && (outExt == L"pdf" || outExt == L"docx" || outExt == L"txt")) {
            std::wstring cmd = L"python -X utf8 -u \"" + scriptPath + L"\" --input \"" + options.inputPath + L"\" --output \"" + outResultPath + L"\"";
            return RunProcessWithRealProgress(cmd, onProgress, outErrorMessage, 0.0);
        }

        // 4. Video / Audio / Photo Conversions via FFmpeg
        double duration = 0.0;
        if (options.category == "video" || inExt == L"mp4" || inExt == L"mkv" || inExt == L"avi" || inExt == L"mov" || inExt == L"webm") {
            duration = GetMediaDurationSeconds(options.inputPath);
        }

        std::wstring ffmpeg = FindFFmpegPath();
        std::wstringstream cmd;
        cmd << ffmpeg << L" -y -hide_banner -loglevel error -progress pipe:1 -i \"" << options.inputPath << L"\" ";

        if (options.extractAudio || outExt == L"mp3" || outExt == L"wav" || outExt == L"aac") {
            cmd << L"-vn ";
            if (outExt == L"mp3") {
                cmd << L"-c:a libmp3lame -q:a 2 ";
            } else if (outExt == L"wav") {
                cmd << L"-c:a pcm_s16le ";
            } else if (outExt == L"aac") {
                cmd << L"-c:a aac -b:a 192k ";
            }
        } else if (outExt == L"gif") {
            cmd << L"-vf \"fps=12,scale=480:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse\" ";
        } else if (options.category == "video") {
            if (options.qualityPreset == "fast") {
                cmd << L"-c:v libx264 -crf 23 -preset ultrafast -c:a aac ";
            } else if (options.qualityPreset == "small") {
                cmd << L"-c:v libx264 -crf 28 -preset slow -c:a aac -b:a 128k ";
            } else if (options.qualityPreset == "original") {
                cmd << L"-c:v copy -c:a copy ";
            } else {
                cmd << L"-c:v libx264 -crf 18 -preset medium -c:a aac -b:a 192k ";
            }
        } else if (options.category == "photo") {
            if (outExt == L"jpg" || outExt == L"jpeg") {
                cmd << L"-q:v 2 ";
            } else if (outExt == L"webp") {
                cmd << L"-quality 90 ";
            }
        }

        cmd << L"\"" << outResultPath << L"\"";

        bool ok = RunProcessWithRealProgress(cmd.str(), onProgress, outErrorMessage, duration);
        if (!ok || !fs::exists(outResultPath)) {
            if (options.qualityPreset == "original" && options.category == "video" && !g_cancelRequested) {
                std::wstringstream fallbackCmd;
                fallbackCmd << ffmpeg << L" -y -hide_banner -loglevel error -progress pipe:1 -i \"" << options.inputPath
                    << L"\" -c:v libx264 -crf 18 -preset fast -c:a aac \"" << outResultPath << L"\"";
                return RunProcessWithRealProgress(fallbackCmd.str(), onProgress, outErrorMessage, duration);
            }
            if (outErrorMessage.empty() && !g_cancelRequested) {
                outErrorMessage = "Conversion did not generate target output.";
            }
            return false;
        }

        return true;
    }
}
