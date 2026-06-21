#pragma once

#include_next <FsHelpers.h>

#include <string>
#include <string_view>

namespace FsHelpers {
inline bool checkFileExtension(const std::string& fileName, const char* extension) {
  return checkFileExtension(std::string_view(fileName), extension);
}
inline bool hasJpgExtension(const std::string& fileName) { return hasJpgExtension(std::string_view(fileName)); }
inline bool hasPngExtension(const std::string& fileName) { return hasPngExtension(std::string_view(fileName)); }
inline bool hasBmpExtension(const std::string& fileName) { return hasBmpExtension(std::string_view(fileName)); }
inline bool hasGifExtension(const std::string& fileName) { return hasGifExtension(std::string_view(fileName)); }
inline bool hasEpubExtension(const std::string& fileName) { return hasEpubExtension(std::string_view(fileName)); }
inline bool hasXtcExtension(const std::string& fileName) { return hasXtcExtension(std::string_view(fileName)); }
inline bool hasTxtExtension(const std::string& fileName) { return hasTxtExtension(std::string_view(fileName)); }
inline bool hasMarkdownExtension(const std::string& fileName) {
  return hasMarkdownExtension(std::string_view(fileName));
}
inline bool hasCssExtension(const std::string& fileName) { return hasCssExtension(std::string_view(fileName)); }
}  // namespace FsHelpers
