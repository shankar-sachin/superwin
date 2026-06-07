#include <catch2/catch_test_macros.hpp>

#include "modules/fileconv/ImageConvertLogic.h"

using namespace superwin;

TEST_CASE("Image format detection from extension or path", "[fileconv]") {
    REQUIRE(ImageFormatFromExtension("png") == ImageFormat::Png);
    REQUIRE(ImageFormatFromExtension(".JPG") == ImageFormat::Jpeg);
    REQUIRE(ImageFormatFromExtension("photo.jpeg") == ImageFormat::Jpeg);
    REQUIRE(ImageFormatFromExtension("C:\\a\\b.TIFF") == ImageFormat::Tiff);
    REQUIRE(ImageFormatFromExtension("icon.ico") == ImageFormat::Ico);
    REQUIRE(ImageFormatFromExtension("notes.txt") == ImageFormat::Unknown);
}

TEST_CASE("Canonical extensions", "[fileconv]") {
    REQUIRE(ExtensionForImageFormat(ImageFormat::Jpeg) == "jpg");
    REQUIRE(ExtensionForImageFormat(ImageFormat::Png) == "png");
    REQUIRE(ExtensionForImageFormat(ImageFormat::Tiff) == "tiff");
}

TEST_CASE("Document type detection", "[fileconv]") {
    REQUIRE(IsImageExtension("a.png"));
    REQUIRE_FALSE(IsImageExtension("a.pdf"));
    REQUIRE(IsWordExtension("report.docx"));
    REQUIRE(IsWordExtension("legacy.doc"));
    REQUIRE(IsPdfExtension("manual.PDF"));
    REQUIRE_FALSE(IsWordExtension("manual.pdf"));
}

TEST_CASE("ReplaceExtension preserves directory and stem", "[fileconv]") {
    REQUIRE(ReplaceExtension("C:\\a\\b.png", "jpg") == "C:\\a\\b.jpg");
    REQUIRE(ReplaceExtension("photo.jpeg", "png") == "photo.png");
    REQUIRE(ReplaceExtension("noext", "png") == "noext.png");
    // A dot in a directory name must not be mistaken for an extension.
    REQUIRE(ReplaceExtension("C:\\v1.2\\file", "pdf") == "C:\\v1.2\\file.pdf");
}
