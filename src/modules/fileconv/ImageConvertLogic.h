// Pure helpers for the File Converter: format detection from a path/extension,
// extension <-> format mapping, and output-path derivation. The actual WIC image
// transcoding and Word COM (DOCX<->PDF) automation live in FileConvertPage; this
// stays free of Windows runtime types so it is unit-testable in superwin_core.
#pragma once

#include <string>
#include <vector>

namespace superwin {

enum class ImageFormat { Unknown, Png, Jpeg, Bmp, Gif, Tiff, Webp, Heic, Ico };

// Map a file extension or full path to an image format. Accepts "png", ".PNG" or
// "C:\\a\\b.png" (case-insensitive). Returns Unknown for non-image extensions.
ImageFormat ImageFormatFromExtension(const std::string& extOrPath);

// Canonical lower-case extension (no dot) for a format, e.g. Jpeg -> "jpg".
std::string ExtensionForImageFormat(ImageFormat f);

// The image formats SuperWin can WRITE (reliable WIC encoders). Decoding accepts
// more (anything WIC can read, incl. ICO/WEBP/HEIC when codecs are present).
std::vector<ImageFormat> EncodableImageFormats();

bool IsImageExtension(const std::string& path);
bool IsWordExtension(const std::string& path);   // .doc / .docx
bool IsPdfExtension(const std::string& path);

// Replace a path's extension (newExt without a leading dot), preserving the
// directory and stem: ReplaceExtension("C:\\a\\b.png","jpg") -> "C:\\a\\b.jpg".
std::string ReplaceExtension(const std::string& path, const std::string& newExt);

}  // namespace superwin
