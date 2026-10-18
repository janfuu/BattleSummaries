#include "Settings.h"

#include "Text.h"

#include <SimpleIni.h>

namespace BSM::Settings
{
	namespace
	{
		Data g_data;
	}

	const Data& Get() { return g_data; }

	void SetVerbose(bool a_on)
	{
		g_data.verbose = a_on;
		spdlog::set_level(a_on ? spdlog::level::debug : spdlog::level::info);
		spdlog::flush_on(a_on ? spdlog::level::debug : spdlog::level::info);
	}

	namespace
	{
		std::map<std::uint32_t, int> g_injurySpells;

		void ResolveInjurySpells(const std::vector<std::string>& a_list)
		{
			g_injurySpells.clear();
			auto* dh = RE::TESDataHandler::GetSingleton();
			if (!dh) return;
			for (const auto& entry : a_list) {
				std::string spec = entry, tag;
				if (const auto eq = spec.find('='); eq != std::string::npos) tag = spec.substr(eq + 1), spec = spec.substr(0, eq);
				const auto bar = spec.find('|');
				if (bar == std::string::npos) continue;
				const auto     plugin = spec.substr(0, bar);
				std::uint32_t  local = 0;
				try {
					local = static_cast<std::uint32_t>(std::stoul(spec.substr(bar + 1), nullptr, 0));
				} catch (...) {
					SKSE::log::warn("Settings: sInjurySpells entry '{}' is not Plugin.esp|0xFormID", entry);
					continue;
				}
				if (!dh->LookupModByName(plugin)) continue;  // that mod is not in this load order: nothing to say
				if (auto* spell = dh->LookupForm<RE::SpellItem>(local, plugin)) g_injurySpells[spell->GetFormID()] = tag == "lasting" ? 2 : 1;
				else SKSE::log::warn("Settings: sInjurySpells: no spell {:#x} in {}", local, plugin);
			}
			if (!g_injurySpells.empty()) SKSE::log::info("Settings: {} injury spells from other mods recognised", g_injurySpells.size());
		}
	}

	int InjurySpell(std::uint32_t a_formId)
	{
		const auto it = g_injurySpells.find(a_formId);
		return it != g_injurySpells.end() ? it->second : 0;
	}

	void LoadText(bool a_force)
	{
		static std::filesystem::file_time_type seen{};
		static bool                            had = false;
		const std::filesystem::path            path{ L"Data/SKSE/Plugins/BattleSummaries_Text.ini" };
		std::error_code                        ec;
		const bool                             exists = std::filesystem::exists(path, ec);
		if (!exists) {
			if (had) {
				had = false;
				Text::SetOverrides({});
				SKSE::log::info("Text: BattleSummaries_Text.ini is gone; the built-in wording is back in force");
			}
			return;
		}
		const auto stamp = std::filesystem::last_write_time(path, ec);
		if (!a_force && had && stamp == seen) return;
		had = true;
		seen = stamp;

		CSimpleIniA ini;
		ini.SetUnicode();
		if (ini.LoadFile(path.c_str()) < 0) {
			SKSE::log::warn("Text: BattleSummaries_Text.ini could not be read; the built-in wording stays in force");
			return;
		}
		std::vector<std::pair<std::string, std::string>> lines;
		CSimpleIniA::TNamesDepend                        sections;
		ini.GetAllSections(sections);
		for (const auto& section : sections) {  // the headings are only for finding things: a line counts under any of them
			CSimpleIniA::TNamesDepend keys;
			ini.GetAllKeys(section.pItem, keys);
			for (const auto& key : keys) {
				const char* value = ini.GetValue(section.pItem, key.pItem, "");
				if (!value || !*value) continue;
				std::string why;
				if (Text::Accepts(key.pItem, value, &why)) lines.emplace_back(key.pItem, value);
				else SKSE::log::warn("Text: the line '{}' is not used: {}", key.pItem, why);
			}
		}
		const auto taken = Text::SetOverrides(lines);
		SKSE::log::info("Text: {} of your own phrases in force (BattleSummaries_Text.ini)", taken);
	}

	void Override(const std::string& a_path, const std::string& a_value)
	{
		if (a_value.empty()) return;
		std::string v = a_value;
		std::ranges::transform(v, v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		const bool on = v == "true" || v == "1" || v == "yes" || v == "on";
		const auto set = [&](bool& a_field) {
			if (a_field != on) SKSE::log::info("Settings: {} = {} (from SkyrimNet's settings page)", a_path, on);
			a_field = on;
		};
		if (a_path == "summary.ignore_friendly_fire") set(g_data.ignoreFriendlyFire);
		else if (a_path == "summary.ignore_harmless_effects") set(g_data.ignoreHarmless);
		else if (a_path == "summary.mention_bystanders") set(g_data.mentionBystanders);
		else if (a_path == "summary.show_numbers") set(g_data.showNumbers);
		else if (a_path == "memory.remember_battles") set(g_data.rememberEvent);
		else if (a_path == "mods.injuries") set(g_data.injuries);
		else if (a_path == "mods.dismemberment") set(g_data.dismemberment);
		else if (a_path == "summary.game_hours") {
			float hours = g_data.summaryGameHours;
			try {
				hours = std::stof(v);
			} catch (...) {
				return;
			}
			hours = std::clamp(hours, 0.25f, 240.0f);
			if (std::fabs(hours - g_data.summaryGameHours) > 0.01f) SKSE::log::info("Settings: {} = {} (from SkyrimNet's settings page)", a_path, hours);
			g_data.summaryGameHours = hours;
		}
	}

	void Load()
	{
		Data      d;
		CSimpleIniA ini;
		ini.SetUnicode();
		const auto rc = ini.LoadFile(L"Data/SKSE/Plugins/BattleSummaries.ini");
		if (rc < 0) {
			SKSE::log::warn("BattleSummaries.ini not found; using defaults");
		} else {
			d.enabled = ini.GetBoolValue("General", "bEnabled", d.enabled);
			d.nearDeathPct = static_cast<float>(ini.GetDoubleValue("General", "fNearDeathHealthShare", d.nearDeathPct));
			d.endGraceSeconds = static_cast<float>(ini.GetDoubleValue("General", "fEndGraceSeconds", d.endGraceSeconds));
			d.mergeGapSeconds = static_cast<float>(ini.GetDoubleValue("General", "fMergeGapSeconds", d.mergeGapSeconds));
			d.range = static_cast<float>(ini.GetDoubleValue("General", "fTrackingRange", d.range));
			d.witnessRange = static_cast<float>(ini.GetDoubleValue("General", "fWitnessRange", d.witnessRange));

			d.summaryGameHours = static_cast<float>(ini.GetDoubleValue("Summary", "fSummaryGameHours", d.summaryGameHours));
			d.showNumbers = ini.GetBoolValue("Summary", "bShowNumbers", d.showNumbers);
			d.ignoreFriendlyFire = ini.GetBoolValue("Summary", "bIgnoreFriendlyFire", d.ignoreFriendlyFire);
			d.ignoreHarmless = ini.GetBoolValue("Summary", "bIgnoreHarmlessEffects", d.ignoreHarmless);
			d.maxOthers = static_cast<int>(ini.GetLongValue("Summary", "iMaxCompanionLines", d.maxOthers));
			d.maxEffects = static_cast<int>(ini.GetLongValue("Summary", "iMaxEffectsPerList", d.maxEffects));

			d.rememberEvent = ini.GetBoolValue("Memory", "bRememberBattles", d.rememberEvent);
			d.minEnemies = static_cast<int>(ini.GetLongValue("Memory", "iMinEnemies", d.minEnemies));
			d.minSeconds = static_cast<float>(ini.GetDoubleValue("Memory", "fMinSeconds", d.minSeconds));

			d.injuries = ini.GetBoolValue("OtherMods", "bInjuries", d.injuries);
			if (const char* list = ini.GetValue("OtherMods", "sInjuryKeywords", nullptr)) {
				d.injuryKeywords.clear();
				std::string word;
				const auto  flush = [&] {
					if (!word.empty()) d.injuryKeywords.push_back(word);
					word.clear();
				};
				for (const char* c = list; *c; ++c) {
					if (*c == ',' || *c == ';' || std::isspace(static_cast<unsigned char>(*c))) flush();
					else word += *c;
				}
				flush();
			}
			d.dismemberment = ini.GetBoolValue("OtherMods", "bDismemberment", d.dismemberment);
			if (const char* list = ini.GetValue("OtherMods", "sInjurySpells", nullptr)) {
				d.injurySpells.clear();
				std::string item;
				const auto  flush = [&] {
					if (!item.empty()) d.injurySpells.push_back(item);
					item.clear();
				};
				for (const char* c = list; *c; ++c) {
					if (*c == ',' || std::isspace(static_cast<unsigned char>(*c))) flush();
					else item += *c;
				}
				flush();
			}
			d.mentionBystanders = ini.GetBoolValue("Summary", "bMentionBystanders", d.mentionBystanders);

			d.verbose = ini.GetBoolValue("Debug", "bVerboseLog", d.verbose);
		}
		d.nearDeathPct = std::clamp(d.nearDeathPct, 0.01f, 0.9f);
		d.endGraceSeconds = std::clamp(d.endGraceSeconds, 1.0f, 120.0f);
		g_data = d;
		SetVerbose(d.verbose);
		ResolveInjurySpells(d.injurySpells);
		LoadText(true);
		SKSE::log::info("Settings: injuries {} ({} keywords), dismemberment {}", d.injuries, d.injuryKeywords.size(), d.dismemberment);
		SKSE::log::info("Settings: enabled {}, near death below {:.0f}%, end grace {}s, merge gap {}s, summary for {} game hours, numbers {}, remember {}",
			d.enabled, d.nearDeathPct * 100.0f, d.endGraceSeconds, d.mergeGapSeconds, d.summaryGameHours, d.showNumbers, d.rememberEvent);
	}
}
