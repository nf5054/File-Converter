#pragma once
#include <string>
#include <vector>
#include <functional>

namespace ConverterEngine {

    struct ConvertOptions {
        std::wstring inputPath;
        std::wstring targetExt;
        std::string category;
        std::string qualityPreset;
        bool extractAudio = false;
        std::string destinationChoice = "same";

        // Compression parameters
        bool isCompressMode = false;
        int pdfQuality = 75;
        int pdfPpi = 150;
        int videoQualityPct = 65;
        std::string videoResolution = "original";
    };

    typedef std::function<void(int percent, const std::string& detail)> ProgressCallback;

    std::wstring DetermineOutputPath(const std::wstring& inputPath, const std::wstring& targetExt, const std::string& destinationChoice, bool isCompress = false);

    bool ExecuteConversion(
        const ConvertOptions& options,
        ProgressCallback onProgress,
        std::wstring& outResultPath,
        std::string& outErrorMessage);

    void CancelCurrentConversion();

    // Helpers
    std::wstring GetExecutableDir();
    std::wstring FindFFmpegPath();
    std::wstring FindFFprobePath();
    std::string WideToUtf8(const std::wstring& wstr);
    std::wstring Utf8ToWide(const std::string& str);
    double GetMediaDurationSeconds(const std::wstring& mediaPath);
}
