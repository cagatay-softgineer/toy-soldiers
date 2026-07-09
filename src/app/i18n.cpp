#include "app/i18n.h"

#include <cstdio>
#include <cstring>

namespace toy {

namespace {

Lang g_lang = Lang::En;

struct Entry {
	const char* key;
	const char* en;
	const char* tr;
};

// Minimal UI + glossary dictionary (v0.6 P1). Expand in v0.9.
const Entry kTable[] = {
	{ "app.title", "Toy Soldiers Tabletop", "Oyuncak Asker Masa Savasi" },
	{ "menu.play_offline", "Play Offline (Hotseat)", "Cevrimdisi Oyna (Hotseat)" },
	{ "menu.host", "Host Lobby", "Lobi Ac (Host)" },
	{ "menu.join", "Join Host", "Host'a Katil" },
	{ "menu.settings", "Settings", "Ayarlar" },
	{ "menu.howto", "How to Play", "Nasil Oynanir" },
	{ "menu.quit", "Quit", "Cikis" },
	{ "menu.continue", "Continue last mode", "Son moda devam" },
	{ "menu.display_name", "Display name", "Gorunen ad" },
	{ "menu.join_ip", "Join IP", "Katilim IP" },
	{ "settings.title", "Settings", "Ayarlar" },
	{ "settings.save", "Save", "Kaydet" },
	{ "settings.back", "Back", "Geri" },
	{ "settings.reset", "Reset to defaults", "Varsayilanlara don" },
	{ "settings.fullscreen", "Fullscreen", "Tam ekran" },
	{ "settings.vsync", "VSync", "VSync" },
	{ "settings.show_fps", "Show FPS", "FPS goster" },
	{ "settings.show_sync", "Show sync generation", "Sync sayacini goster" },
	{ "settings.high_contrast", "High-contrast UI", "Yuksek kontrast arayuz" },
	{ "settings.language", "Language", "Dil" },
	{ "settings.auto_orbit", "Auto-orbit camera", "Otomatik kamera" },
	{ "lobby.start", "Start Match", "Maci Baslat" },
	{ "lobby.ready", "Ready", "Hazir" },
	{ "lobby.leave", "Leave to Menu", "Menuye don" },
	{ "match.your_turn", "YOUR TURN — play one card", "SIRA SENDE — bir kart oyna" },
	{ "match.pause", "Pause / Menu", "Duraklat / Menu" },
	{ "match.hand", "Hand", "El" },
	{ "match.target", "Target:", "Hedef:" },
	{ "match.play", "Play Selected Card", "Secili Karti Oyna" },
	{ "match.auto", "Auto Play", "Otomatik Oyna" },
	{ "match.end_turn", "End Turn", "Sirayi Bitir" },
	{ "pause.title", "Paused", "Duraklatildi" },
	{ "pause.resume", "Resume", "Devam" },
	{ "pause.resign", "Resign / Leave match", "Pes et / Mactan cik" },
	{ "pause.confirm", "Leave this match and return to menu?", "Maci birakip menuye donulsun mu?" },
	{ "pause.yes", "Yes, leave", "Evet, cik" },
	{ "pause.no", "Cancel", "Iptal" },
	{ "results.play_again", "Play again", "Tekrar oyna" },
	{ "results.change_loadout", "Change loadout", "Yuklemeyi degistir" },
	{ "results.menu", "Main Menu", "Ana Menu" },
	{ "results.rematch", "Rematch", "Rovans" },
	{ "glossary.title", "Glossary", "Sozluk" },
	{ "glossary.shield", "Shield: halves incoming attack damage (round up). Blocks flood chip fully.",
	  "Kalkan: gelen saldiri hasarini yariya indirir (yukari yuvarlar). Sel hasarini tamamen engeller." },
	{ "glossary.adjacent", "Adjacent: only neighboring seats on the table (N-E-S-W ring).",
	  "Komsu: masadaki yan koltuklar (K-D-G-B halkasi)." },
	{ "glossary.world_event", "World Event: map-driven effect (sandstorm, rain, flood, cat).",
	  "Dunya Olayi: harita kaynakli etki (kum firtinasi, yagmur, sel, kedi)." },
	{ "glossary.host", "Host: lobby owner; authoritative game state and validation.",
	  "Host: lobi sahibi; yetkili oyun durumu ve dogrulama." },
	{ "kw.Shield", "Shield", "Kalkan" },
	{ "kw.Pierce", "Pierce", "Delici" },
	{ "kw.Draw", "Draw", "Cek" },
	{ "kw.Heal", "Heal", "Iyilestir" },
	{ "kw.AdjacentOnly", "AdjacentOnly", "SadeceKomsu" },
	{ "kw.Attack", "Attack", "Saldiri" },
	{ "kw.Defense", "Defense", "Defans" },
	{ "kw.Tactic", "Tactic", "Taktik" },
	{ "err.not_your_turn", "Not your turn.", "Sira sende degil." },
	{ "err.sandstorm", "Sandstorm: adjacent targets only.", "Kum firtinasi: sadece komsu hedefler." },
	{ "err.eliminated", "Target is eliminated.", "Hedef elendi." },
};

const char* pick(const Entry& e)
{
	return g_lang == Lang::Tr ? e.tr : e.en;
}

} // namespace

void i18nSetLang(Lang lang)
{
	g_lang = lang;
}

Lang i18nLang()
{
	return g_lang;
}

const char* tr(const char* key)
{
	if (!key) {
		return "";
	}
	for (const Entry& e : kTable) {
		if (std::strcmp(e.key, key) == 0) {
			return pick(e);
		}
	}
	return key;
}

const char* trKeyword(const char* keyword)
{
	if (!keyword) {
		return "";
	}
	char buf[64];
	std::snprintf(buf, sizeof(buf), "kw.%s", keyword);
	for (const Entry& e : kTable) {
		if (std::strcmp(e.key, buf) == 0) {
			return pick(e);
		}
	}
	return keyword;
}

} // namespace toy
