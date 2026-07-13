#pragma once

namespace toy {

enum class Lang : int {
	En = 0,
	Tr = 1,
	// v1.2 #157: partial stubs (core menu strings translated, everything else falls
	// back to English) — kept for the Erasmus narrative option.
	De = 2,
	It = 3,
};

void i18nSetLang(Lang lang);
Lang i18nLang();

// Lookup key → localized C string (never null).
const char* tr(const char* key);

// Convenience: card class / glossary keys live in i18n table.
const char* trKeyword(const char* keyword);

} // namespace toy
