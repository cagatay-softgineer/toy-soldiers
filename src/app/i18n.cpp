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
	{ "menu.credits", "Credits", "Jenerik" },
	{ "menu.quit", "Quit", "Cikis" },
	{ "menu.continue", "Continue last mode", "Son moda devam" },
	{ "menu.display_name", "Display name", "Gorunen ad" },
	{ "menu.join_ip", "Join IP", "Katilim IP" },
	{ "menu.match_setup", "Match setup", "Mac ayarlari" },
	{ "menu.mode", "Game mode", "Oyun modu" },
	{ "menu.ai_level", "AI difficulty", "YZ zorlugu" },
	{ "menu.turn_timer", "Turn timer", "Tur suresi" },
	{ "menu.free_targeting", "Free targeting (off = adjacent only)", "Serbest hedefleme (kapali = sadece komsu)" },
	{ "menu.parade_rest", "Parade rest (stand soldiers up each round)", "Hazir ol (askerler her tur ayaga kalkar)" },
	{ "menu.codex", "Card Codex", "Kart Kodeksi" },
	{ "menu.join_lan", "Join a LAN game", "LAN oyununa katil" },
	{ "menu.room_code", "Room code", "Oda kodu" },
	{ "menu.join_code", "Join by code", "Kodla katil" },
	{ "menu.lan_scanning", "Scanning for LAN games…", "LAN oyunlari araniyor…" },
	{ "menu.recent_hosts", "Recent hosts", "Son sunucular" },
	{ "menu.manual_ip", "Manual IP (advanced)", "Elle IP gir (gelismis)" },
	{ "lobby.copy", "Copy", "Kopyala" },
	{ "lobby.kick", "Kick", "At" },
	{ "lobby.locked", "Close lobby to new players", "Lobiyi yeni oyunculara kapat" },
	{ "lobby.waiting_ready", "Waiting for all players to ready up…", "Tum oyuncularin hazir olmasi bekleniyor…" },
	{ "lobby.chat", "Chat", "Sohbet" },
	{ "lobby.send", "Send", "Gonder" },
	{ "results.vote", "Vote rematch", "Rovans icin oy ver" },
	{ "results.vote_sent", "Vote sent — waiting for others…", "Oy gonderildi — digerleri bekleniyor…" },
	{ "results.vote_hint", "Starts when every connected player votes.", "Tum bagli oyuncular oy verince baslar." },
	{ "codex.title", "Card Codex", "Kart Kodeksi" },
	{ "codex.header", "Every card in the toybox", "Oyuncak kutusundaki tum kartlar" },
	{ "events.title", "Event history", "Olay gecmisi" },
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
	{ "settings.reduced_motion", "Reduced motion", "Azaltilmis hareket" },
	{ "settings.coach_tips", "Coach tips (first 3 matches)", "Kocluk ipuclari (ilk 3 mac)" },
	{ "settings.export_json", "Export settings JSON", "Ayarlari JSON disa aktar" },
	{ "settings.import_json", "Import settings JSON", "Ayarlari JSON ice aktar" },
	{ "credits.title", "Credits", "Jenerik" },
	{ "credits.back", "Back to menu", "Menuye don" },
	{ "credits.updates", "Project page / check updates", "Proje sayfasi / guncelleme kontrol" },
	{ "menu.tutorial", "Tutorial (first match)", "Egitim (ilk mac)" },
	{ "menu.tutorial_hint", "New here? The 2-minute tutorial unlocks Sandbox mode.",
	  "Yeni misin? 2 dakikalik egitim Sandbox modunu acar." },
	{ "mode.locked_sandbox", "  (finish the tutorial)", "  (once egitimi bitir)" },
	{ "tut.title", "Tutorial", "Egitim" },
	{ "tut.0", "Welcome to the table! Your plastic platoon guards the tower with the ring under it. "
	  "Destroy the enemy tower before yours falls.",
	  "Masaya hos geldin! Plastik mangan altinda halka olan kuleyi koruyor. "
	  "Kendi kulen dusmeden dusman kulesini yik." },
	{ "tut.1", "The bottom panel is your HAND. Each turn you play exactly ONE card, then the turn passes.",
	  "Alttaki panel senin ELIN. Her tur tam olarak BIR kart oynarsin, sonra sira gecer." },
	{ "tut.2", "Attack cards need a TARGET — it is picked automatically, and the golden ring on the table "
	  "shows who you are aiming at. Attack buttons preview the damage.",
	  "Saldiri kartlari HEDEF ister — otomatik secilir ve masadaki altin halka kimi hedefledigini gosterir. "
	  "Saldiri dugmeleri hasari onceden gosterir." },
	{ "tut.3", "Your move: click an Attack card, then 'Play Selected Card' (or press Enter).",
	  "Sira sende: bir Saldiri kartina tikla, sonra 'Secili Karti Oyna' (veya Enter)." },
	{ "tut.4", "Nice hit! Defense cards give SHIELD (halves damage), Tactic cards draw and heal. "
	  "Play defensively when your HP bar gets low.",
	  "Guzel vurus! Defans kartlari KALKAN verir (hasari yarilar), Taktik kartlari cektirir ve iyilestirir. "
	  "HP barin dusunce savunmaya gec." },
	{ "tut.5", "Maps throw WORLD EVENTS at everyone — storms, floods, the cat. Event cards let you cast "
	  "your own. The Card Codex on the menu lists everything.",
	  "Haritalar herkese DUNYA OLAYLARI yagdirir — firtina, sel, kedi. Olay kartlariyla kendininkini "
	  "atarsin. Menudeki Kart Kodeksi hepsini listeler." },
	{ "tut.6", "You know everything you need. Finish the fight — win or lose, the toybox is yours after this!",
	  "Gereken her seyi biliyorsun. Dovusu bitir — kazan ya da kaybet, oyuncak kutusu artik senin!" },
	{ "tut.next", "Next", "Devam" },
	{ "tut.skip", "skip tutorial", "egitimi atla" },
	{ "tut.wait_play", "waiting for your card…", "kartini oynamani bekliyor…" },
	{ "tut.wait_win", "finish the match to complete the tutorial", "egitimi bitirmek icin maci tamamla" },
	{ "menu.profile", "Profile & Badges", "Profil ve Rozetler" },
	{ "profile.title", "Profile", "Profil" },
	{ "profile.matches", "Matches", "Maclar" },
	{ "profile.wins", "Wins", "Galibiyetler" },
	{ "profile.fav_map", "Favorite map", "Favori harita" },
	{ "profile.hosted", "Lobbies hosted:", "Acilan lobiler:" },
	{ "profile.tut_done", "tutorial complete", "egitim tamamlandi" },
	{ "profile.tut_todo", "tutorial not done yet", "egitim henuz bitmedi" },
	{ "profile.badges", "Badges", "Rozetler" },
	{ "profile.missions", "Challenge missions", "Gorevler" },
	{ "profile.history", "Recent matches (last 20)", "Son maclar (son 20)" },
	{ "profile.no_history", "No matches recorded yet — go play!", "Henuz mac kaydi yok — hadi oyna!" },
	{ "badge.graduate", "Graduate — finish the tutorial", "Mezun — egitimi bitir" },
	{ "badge.first_win", "First Blood (plastic) — win a match", "Ilk Zafer — bir mac kazan" },
	{ "badge.veteran", "Veteran — win 10 matches", "Kidemli — 10 mac kazan" },
	{ "badge.regular", "Regular — play 10 matches", "Mudavim — 10 mac oyna" },
	{ "badge.devoted", "Devoted — play 25 matches", "Sadik — 25 mac oyna" },
	{ "badge.party_host", "Party Host — host 5 lobbies", "Parti Sahibi — 5 lobi ac" },
	{ "mission.storm", "Storm Winner — win a match where a sandstorm raged", "Firtina Galibi — kum firtinali bir maci kazan" },
	{ "mission.medic", "Field Medic — heal 10+ HP in one match", "Sahra Hekimi — tek macta 10+ HP iyilestir" },
	{ "mission.untouchable", "Untouchable — win with 75%+ tower HP", "Dokunulmaz — kule HP %75+ ile kazan" },
	{ "timeline.title", "Match timeline", "Mac zaman cizelgesi" },
	{ "timeline.scrub", "Event", "Olay" },
	{ "coach.dismiss", "Got it", "Anladim" },
	{ "coach.tip0", "Tip: Pick a target first, then play one card. Enter also plays.",
	  "Ipucu: Once hedef sec, sonra bir kart oyna. Enter da oynar." },
	{ "coach.tip1", "Tip: Shield halves attack damage. Flood chip is blocked fully by shield.",
	  "Ipucu: Kalkan saldiri hasarini yariya indirir. Sel hasarini tamamen engeller." },
	{ "coach.tip2", "Tip: Sandstorm forces adjacent targets only — check the world banner.",
	  "Ipucu: Kum firtinasi sadece komsu hedeflere izin verir — dunya banerine bak." },
	{ "lobby.start", "Start Match", "Maci Baslat" },
	{ "lobby.start_offline", "Start Offline Match", "Cevrimdisi Maci Baslat" },
	{ "lobby.ready", "Ready", "Hazir" },
	{ "lobby.leave", "Leave to Menu", "Menuye don" },
	{ "lobby.world_events", "World events", "Dunya olaylari" },
	{ "lobby.fill_ai", "Fill empty seats with AI", "Bos koltuklari YZ ile doldur" },
	{ "lobby.tower_type", "Tower type (gameplay):", "Kule tipi (oynanis):" },
	{ "settings.master_vol", "Master volume", "Ana ses" },
	{ "settings.sfx_vol", "SFX volume", "Efekt sesi" },
	{ "settings.music_vol", "Music volume", "Muzik sesi" },
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
	{ "kw.AOE", "AOE", "Alan" },
	{ "kw.Attack", "Attack", "Saldiri" },
	{ "kw.Defense", "Defense", "Defans" },
	{ "kw.Tactic", "Tactic", "Taktik" },
	{ "kw.Event", "Event", "Olay" },
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
