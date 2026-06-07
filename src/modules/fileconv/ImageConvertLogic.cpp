#include "modules/fileconv/ImageConvertLogic.h"

#include <algorithm>
#include <cctype>

namespace superwin {
namespace {

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Extract the lower-case extension (no dot) from a path or bare extension.
std::string ExtOf(const std::string& s) {
    const auto slash = s.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? s : s.substr(slash + 1);
    const auto dot = name.find_last_of('.');
    std::string ext = dot == std::string::npos ? name : name.substr(dot + 1);
    return Lower(ext);
}

}  // namespace

ImageFormat ImageFormatFromExtension(const std::string& extOrPath) {
    const std::string e = ExtOf(extOrPath);
    if (e == "png") return ImageFormat::Png;
    if (e == "jpg" || e == "jpeg" || e == "jpe") return ImageFormat::Jpeg;
    if (e == "bmp" || e == "dib") return ImageFormat::Bmp;
    if (e == "gif") return ImageFormat::Gif;
    if (e == "tif" || e == "tiff") return ImageFormat::Tiff;
    if (e == "webp") return ImageFormat::Webp;
    if (e == "heic" || e == "heif") return ImageFormat::Heic;
    if (e == "ico") return ImageFormat::Ico;
    return ImageFormat::Unknown;
}

std::string ExtensionForImageFormat(ImageFormat f) {
    switch (f) {
        case ImageFormat::Png:  return "png";
        case ImageFormat::Jpeg: return "jpg";
        case ImageFormat::Bmp:  return "bmp";
        case ImageFormat::Gif:  return "gif";
        case ImageFormat::Tiff: return "tiff";
        case ImageFormat::Webp: return "webp";
        case ImageFormat::Heic: return "heic";
        case ImageFormat::Ico:  return "ico";
        case ImageFormat::Unknown: break;
    }
    return "";
}

std::vector<ImageFormat> EncodableImageFormats() {
    // WIC has reliable built-in encoders for these. (WebP has only a decoder on
    // Windows, so it is a readable source but not an output target; HEIC encodes
    // only when the system HEIF/HEVC extension is installed -- handled gracefully.)
    return {ImageFormat::Png, ImageFormat::Jpeg, ImageFormat::Bmp, ImageFormat::Gif,
            ImageFormat::Tiff, ImageFormat::Heic};
}

bool IsImageExtension(const std::string& path) {
    return ImageFormatFromExtension(path) != ImageFormat::Unknown;
}

bool IsWordExtension(const std::string& path) {
    const std::string e = ExtOf(path);
    return e == "doc" || e == "docx";
}

bool IsPdfExtension(const std::string& path) {
    return ExtOf(path) == "pdf";
}

std::string ReplaceExtension(const std::string& path, const std::string& newExt) {
    const auto slash = path.find_last_of("/\\");
    const auto dot = path.find_last_of('.');
    // Only treat the dot as an extension separator if it comes after the last slash.
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        return path.substr(0, dot + 1) + newExt;
    return path + "." + newExt;
}

}  // namespace superwin
