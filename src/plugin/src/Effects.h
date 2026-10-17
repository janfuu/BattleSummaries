#pragma once

// Reads a magic effect the way a fighter would describe it afterwards: "burned by fire", "slowed", "shielded by
// magical armor". Works from the effect's own data (archetype, actor value, resistance, keywords), so spells from
// other mods classify too; anything it cannot place is named by the effect itself.

namespace BSM::Effects
{
	struct Kind
	{
		std::string label;
		bool        hostile{ false };
		bool        heal{ false };  // restores health: told through the healing lines, with the amount
		bool        damage{ false };  // lowers health (TakesHealth): told only when it took some, with ignoreHarmless
	};

	// a_source: the spell, potion, poison or enchantment that carried the effect, when known.
	[[nodiscard]] std::optional<Kind> Classify(const RE::EffectSetting* a_effect, const RE::MagicItem* a_source, bool a_selfCast);

	[[nodiscard]] bool RestoresHealth(const RE::EffectSetting* a_effect);

	// A damage effect: it lowers health. Absorb Health (drained of life), and any detrimental health modifier: Damage
	// Health, a fire, frost or shock spell's damage, a poison's. Judged from the effect's data, not from what it is called.
	[[nodiscard]] bool TakesHealth(const RE::EffectSetting* a_effect);

	// An injury from an injury mod: the effect carries one of the keywords listed in the ini. Main thread only.
	[[nodiscard]] bool IsInjury(const RE::EffectSetting* a_effect);

	// What to call it: the effect's own name when that says "injury" ("Leg Injury"), else its spell's ("Major Injury"
	// on an effect named "Reduced Health").
	[[nodiscard]] std::string InjuryName(const RE::EffectSetting* a_effect, const RE::MagicItem* a_source);
}
