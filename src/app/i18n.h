#pragma once

namespace toy {

enum class Lang : int {
	En = 0,
	Tr = 1,
};

void i18nSetLang(Lang lang);
Lang i18nLang();

// Lookup key → localized C string (never null).
const char* tr(const char* key);

// Convenience: card class / glossary keys live in i18n table.
const char* trKeyword(const char* keyword);

} // namespace toy
