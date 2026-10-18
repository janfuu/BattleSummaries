#pragma once

#include <string>
#include <vector>

namespace BSM::Settings
{
	struct Data
	{
		bool  enabled{ true };
		float nearDeathPct{ 0.15f };    // below this share of health an actor counts as near death
		float endGraceSeconds{ 6.0f };  // the battle is over once nobody in it has been in combat for this long
		float mergeGapSeconds{ 20.0f }; // fighting that resumes within this long continues the same battle
		float summaryGameHours{ 6.0f };   // how long after a battle its summary is still given to dialogue
		float range{ 10000.0f };        // actors further than this from the player are not tracked
		float witnessRange{ 3500.0f };  // bystanders this close at the end know what happened
		bool  showNumbers{ false };
		bool  ignoreFriendlyFire{ false };  // false: harm from one's own side is told as an accident; true: left out
		bool  ignoreHarmless{ true };       // a damage effect that took no health is left out (an aura pulsing over everyone)
		int   maxOthers{ 6 };
		int   maxEffects{ 6 };

		bool  rememberEvent{ true };    // register one persistent SkyrimNet event per significant battle
		int   minEnemies{ 3 };
		float minSeconds{ 30.0f };

		// what other mods add
		bool                     injuries{ true };       // tell injuries from injury mods
		std::vector<std::string> injuryKeywords{ "MAG_MagicInjurySpell", "WCT_Injury_Keyword" };  // Blade and Blunt, Wildcat
		bool                     dismemberment{ true };  // tell beheadings and severed limbs
		// injuries an injury mod marks with a spell rather than a keyword: "Plugin.esp|0xFormID", "=lasting" for a lasting one
		std::vector<std::string> injurySpells{ "FollowerDeathChance.esp|0x80A", "FollowerDeathChance.esp|0x814", "FollowerDeathChance.esp|0x815",
			"FollowerDeathChance.esp|0x816", "FollowerDeathChance.esp|0x817", "FollowerDeathChance.esp|0x818", "FollowerDeathChance.esp|0x821=lasting" };
		bool                     mentionBystanders{ false };  // tell those killed without fighting; off: leave them out

		bool verbose{ false };
	};

	[[nodiscard]] const Data& Get();
	void                      Load();  // Data/SKSE/Plugins/BattleSummaries.ini; missing keys keep their defaults
	void                      SetVerbose(bool a_on);

	// The player's own wording: Data/SKSE/Plugins/BattleSummaries_Text.ini, read when it is new or has changed.
	// Lines it holds replace the built-in phrases (Text.h); the log says how many were taken and names any refused.
	void LoadText(bool a_force = false);

	// sInjurySpells resolved to the spells in this load order: 0 = not an injury, 1 = an injury, 2 = a lasting one.
	// Main thread, after the game data has loaded.
	[[nodiscard]] int InjurySpell(std::uint32_t a_formId);

	// A setting made on SkyrimNet's own settings page (its path there, its value as text). Empty = not set there:
	// the ini's value stands. Main thread.
	void Override(const std::string& a_path, const std::string& a_value);
}
