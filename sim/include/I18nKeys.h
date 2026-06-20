#pragma once

// Stub for generated I18nKeys.h - provides minimal definitions for emulator build
// In the real firmware, this would be generated from translation files

enum class Language {
  EN = 0,
  ES,
  FR,
  DE,
  IT,
  PT,
  RU,
  ZH_CN,
  ZH_TW,
  JA,
  KO,
  PL,
  TR,
};

constexpr int V1_LANGUAGE_COUNT = 14;

// Array of Language values for V1 compatibility
const Language V1_LANGUAGES[] = {
  Language::EN, Language::ES, Language::FR, Language::DE,
  Language::IT, Language::PT, Language::RU, Language::ZH_CN,
  Language::ZH_TW, Language::JA, Language::KO, Language::PL,
  Language::TR
};

// Minimal StrId enum - can be extended as needed
enum class StrId {
  INVALID = -1,
  // Add more as needed
};

