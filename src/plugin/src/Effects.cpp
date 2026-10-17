#include "Effects.h"

#include "Settings.h"
#include "Text.h"

namespace BSM::Effects
{
	namespace
	{
		using AV = RE::ActorValue;
		using Arch = RE::EffectSetting::Archetype;
		using Flag = RE::EffectSetting::EffectSettingData::Flag;

		bool ModifiesValue(Arch a_arch)
		{
			return a_arch == Arch::kValueModifier || a_arch == Arch::kPeakValueModifier || a_arch == Arch::kDualValueModifier || a_arch == Arch::kAbsorb ||
				   a_arch == Arch::kAccumulateMagnitude;
		}

		// Something a fighter casts, drinks, shouts or strikes with: the carriers whose effects are worth naming even when
		// this file cannot place them. Anything else (or no known carrier at all) is some mod's own machinery.
		bool Castable(const RE::MagicItem* a_source)
		{
			if (!a_source) return false;
			switch (a_source->GetSpellType()) {
			case RE::MagicSystem::SpellType::kSpell:
			case RE::MagicSystem::SpellType::kPower:
			case RE::MagicSystem::SpellType::kLesserPower:
			case RE::MagicSystem::SpellType::kVoicePower:
			case RE::MagicSystem::SpellType::kPoison:
			case RE::MagicSystem::SpellType::kPotion:
			case RE::MagicSystem::SpellType::kScroll:
			case RE::MagicSystem::SpellType::kStaffEnchantment:
			case RE::MagicSystem::SpellType::kEnchantment:
				return true;
			default:
				return false;
			}
		}

		// the phrase's key in Text
		const char* ResistLabel(AV a_av)
		{
			switch (a_av) {
			case AV::kResistFire: return "fx.resist_fire";
			case AV::kResistFrost: return "fx.resist_frost";
			case AV::kResistShock: return "fx.resist_shock";
			case AV::kResistMagic: return "fx.resist_magic";
			case AV::kPoisonResist: return "fx.resist_poison";
			case AV::kResistDisease: return "fx.resist_disease";
			default: return nullptr;
			}
		}
	}

	bool RestoresHealth(const RE::EffectSetting* a_effect)
	{
		if (!a_effect) return false;
		const auto& d = a_effect->data;
		return d.archetype == Arch::kValueModifier && d.primaryAV == AV::kHealth && d.flags.none(Flag::kDetrimental, Flag::kHostile, Flag::kRecover);
	}

	bool TakesHealth(const RE::EffectSetting* a_effect)
	{
		if (!a_effect) return false;
		const auto& d = a_effect->data;
		if (d.primaryAV != AV::kHealth || !ModifiesValue(d.archetype)) return false;
		return d.archetype == Arch::kAbsorb || d.flags.any(Flag::kDetrimental);
	}

	bool IsInjury(const RE::EffectSetting* a_effect)
	{
		const auto& cfg = Settings::Get();
		if (!a_effect || !cfg.injuries) return false;
		// both mods hang hidden helper effects on the same keyword ("Injury Message", "Leg Injury - Running"): the
		// injury is the one the player is shown
		if (a_effect->data.flags.any(Flag::kHideInUI)) return false;
		for (const auto& kw : cfg.injuryKeywords) {
			if (a_effect->HasKeywordString(kw)) return true;
		}
		return false;
	}

	std::string InjuryName(const RE::EffectSetting* a_effect, const RE::MagicItem* a_source)
	{
		const auto text = [](const char* a_s) { return a_s ? std::string(a_s) : std::string(); };
		const auto own = text(a_effect ? a_effect->GetFullName() : nullptr);
		const auto from = text(a_source ? a_source->GetFullName() : nullptr);
		const auto saysInjury = [](std::string a_s) {
			std::ranges::transform(a_s, a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_s.find("injur") != std::string::npos || a_s.find("wound") != std::string::npos;
		};
		if (saysInjury(own) || from.empty()) return own.empty() ? Text::Get("injury.unnamed") : own;
		return from;
	}

	// what Classify calls it
	static std::optional<Kind> Place(const RE::EffectSetting* a_effect, const RE::MagicItem* a_source, bool a_selfCast)
	{
		if (!a_effect) return std::nullopt;
		const auto& d = a_effect->data;
		const char* name = a_effect->GetFullName();
		// hidden and nameless effects are other mods' plumbing, not something a fighter would notice
		if (d.flags.any(Flag::kHideInUI) || !name || !*name) return std::nullopt;
		if (a_source) {
			const auto type = a_source->GetSpellType();
			if (type == RE::MagicSystem::SpellType::kAbility || type == RE::MagicSystem::SpellType::kAddiction) return std::nullopt;
			if (a_source->GetCastingType() == RE::MagicSystem::CastingType::kConstantEffect) return std::nullopt;  // worn enchantments
		}

		const auto arch = d.archetype;
		const auto av = d.primaryAV;
		const auto has = [&](std::string_view a_kw) { return a_effect->HasKeywordString(a_kw); };
		const bool bad = d.flags.any(Flag::kDetrimental, Flag::kHostile);

		if (RestoresHealth(a_effect)) return Kind{ Text::Get("fx.healed"), false, true };

		switch (arch) {
		case Arch::kParalysis: return Kind{ Text::Get("fx.paralyzed"), true };
		case Arch::kDemoralize: return Kind{ Text::Get("fx.fear"), true };
		case Arch::kFrenzy: return Kind{ Text::Get("fx.frenzy"), true };
		case Arch::kCalm: return Kind{ Text::Get("fx.calm"), true };
		case Arch::kStagger: return Kind{ Text::Get("fx.staggered"), true };
		case Arch::kDisarm: return Kind{ Text::Get("fx.disarmed"), true };
		case Arch::kTurnUndead: return Kind{ Text::Get("fx.turned"), true };
		case Arch::kSoulTrap: return Kind{ Text::Get("fx.soul_trapped"), true };
		case Arch::kBanish: return Kind{ Text::Get("fx.banished"), true };
		case Arch::kInvisibility: return Kind{ Text::Get("fx.invisible"), false };
		case Arch::kRally: return Kind{ Text::Get("fx.emboldened"), false };
		case Arch::kEtherealize: return Kind{ Text::Get("fx.ethereal"), false };
		case Arch::kCloak: return bad && !a_selfCast ? Kind{ Text::Get("fx.cloak_hostile"), true } : Kind{ Text::Get("fx.cloak"), false };
		// casting these is not something done to the target
		case Arch::kSummonCreature:
		case Arch::kReanimate:
		case Arch::kCommandSummoned:
		case Arch::kLight:
		case Arch::kScript:
		case Arch::kSlowTime:
			return std::nullopt;
		default:
			break;
		}

		if (bad) {
			const bool poison = d.resistVariable == AV::kPoisonResist || (a_source && a_source->IsPoison());
			if (poison) return Kind{ Text::Get("fx.poisoned"), true };
			if (d.resistVariable == AV::kResistDisease) return Kind{ Text::Get("fx.diseased"), true };
			if (has("MagicSlow") || av == AV::kSpeedMult) return Kind{ Text::Get("fx.slowed"), true };
			if (has("MagicParalysis")) return Kind{ Text::Get("fx.paralyzed"), true };
			if (has("MagicDamageFire") || d.resistVariable == AV::kResistFire) return Kind{ Text::Get("fx.burned"), true };
			if (has("MagicDamageFrost") || d.resistVariable == AV::kResistFrost) return Kind{ Text::Get("fx.frozen"), true };
			if (has("MagicDamageShock") || d.resistVariable == AV::kResistShock) return Kind{ Text::Get("fx.shocked"), true };
			if (ModifiesValue(arch)) {
				if (arch == Arch::kAbsorb) return Kind{ Text::Get("fx.drained_life"), true };
				if (av == AV::kHealth) return Kind{ Text::Get("fx.hurt"), true };
				if (av == AV::kStamina) return Kind{ Text::Get("fx.drained_stamina"), true };
				if (av == AV::kMagicka) return Kind{ Text::Get("fx.drained_magicka"), true };
				if (av == AV::kDamageResist) return Kind{ Text::Get("fx.armor_weakened"), true };
			}
			// unplaced: named by the effect itself, when a spell, poison or weapon carried it
			if (!Castable(a_source)) return std::nullopt;
			return Kind{ Text::Fill("fx.afflicted", { { "effect", name } }), true };
		}

		if (has("MagicWard")) return Kind{ Text::Get("fx.warded"), false };
		if (has("MagicArmorSpell") || (ModifiesValue(arch) && av == AV::kDamageResist)) return Kind{ Text::Get("fx.armored"), false };
		if (ModifiesValue(arch)) {
			if (const auto* r = ResistLabel(av)) return Kind{ Text::Get(r), false };
			if (av == AV::kHealth) return Kind{ Text::Get("fx.health_fortified"), false };
			if (av == AV::kStamina) return Kind{ Text::Get("fx.stamina_restored"), false };
			if (av == AV::kMagicka) return Kind{ Text::Get("fx.magicka_restored"), false };
		}
		// an unplaced helpful effect: worth a line when someone else gave it, noise when it is the caster's own
		if (a_selfCast || !Castable(a_source)) return std::nullopt;
		return Kind{ Text::Fill("fx.aided", { { "effect", name } }), false };
	}

	std::optional<Kind> Classify(const RE::EffectSetting* a_effect, const RE::MagicItem* a_source, bool a_selfCast)
	{
		auto kind = Place(a_effect, a_source, a_selfCast);
		if (kind && kind->hostile) kind->damage = TakesHealth(a_effect);
		return kind;
	}
}
