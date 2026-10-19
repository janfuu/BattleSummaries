// Offline tests of the battle model and its narrative: no game, no SkyrimNet. Run by build.ps1 before the DLL is
// accepted. Each scenario replays a fight as the tracker would report it and checks what an NPC is then told.

#define _CRT_SECURE_NO_WARNINGS
#include "../src/Core.h"
#include "../src/Narrative.h"
#include "../src/Text.h"

#include <cstdio>
#include <format>
#include <cstdlib>
#include <set>
#include <string>

using namespace BSM;

namespace
{
	int g_failed = 0;
	int g_checks = 0;

	void Check(bool a_ok, const char* a_what, const std::string& a_text)
	{
		++g_checks;
		if (a_ok) return;
		++g_failed;
		std::printf("FAIL: %s\n----\n%s\n----\n", a_what, a_text.c_str());
	}

	bool Has(const std::string& a_text, const char* a_needle) { return a_text.find(a_needle) != std::string::npos; }

	constexpr ActorId kHero = 0x14, kLydia = 0xA2C94, kChief = 0xFF000801, kBandit1 = 0xFF000802, kBandit2 = 0xFF000803, kMage = 0xFF000804,
					  kAtronach = 0xFF000805, kGuard = 0xFF000806;

	ActorInfo Info(ActorId a_id, const char* a_name, Side a_side, float a_hp, ActorId a_master = 0)
	{
		return { a_id, a_name, a_side, a_id == kHero, a_master, a_hp };
	}

	// what the tracker reads off a damage effect: whether it was doing anything (a magnitude, its conditions met), what
	// carried it, how long it lasts
	EffectFacts Damaging(bool a_inForce, std::uint32_t a_spell = 0, double a_lasts = 0.0) { return { a_spell, true, a_inForce, a_lasts }; }

	// The fight the mod exists for: Lydia is beaten into bleedout by the chief, Kaira kills the chief and heals her.
	Battle Rescue()
	{
		Battle b;
		b.id = 1;
		b.startedAt = 100.0;
		b.location = "Bleak Falls Barrow";
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
		b.Join(Info(kChief, "Bandit Chief", Side::kEnemy, 400));
		b.Join(Info(kBandit1, "Bandit", Side::kEnemy, 100));
		b.Join(Info(kBandit2, "Bandit", Side::kEnemy, 100));
		b.Join(Info(kMage, "Bandit Mage", Side::kEnemy, 80));

		b.Damage(kBandit1, kLydia, 100, 0.0f, 103);
		b.Death(kBandit1, kLydia, 103);
		b.Death(kBandit1, kLydia, 103.1);  // the game's second report of the same death
		b.Effect(kLydia, kMage, "burned by fire", "Flames", true);
		b.Effect(kLydia, kMage, "burned by fire", "Flames", true);
		b.Damage(kLydia, kMage, 40, 0.86f, 104);
		b.Damage(kLydia, kChief, 150, 0.36f, 106);
		b.Damage(kLydia, kChief, 90, 0.06f, 108);
		b.Down(kLydia, 108.2);
		b.Damage(kHero, kBandit2, 30, 0.85f, 109);
		b.Effect(kHero, kHero, "shielded by magical armor", "Stoneflesh", false);
		b.Damage(kBandit2, kHero, 100, 0.0f, 110);
		b.Death(kBandit2, kHero, 110);
		b.Damage(kMage, kHero, 80, 0.0f, 112);
		b.Death(kMage, kHero, 112);
		b.Damage(kChief, kHero, 400, 0.0f, 118);
		b.Death(kChief, kHero, 118);
		b.Heal(kLydia, kHero, 240, "Healing Hands", 121);
		b.Up(kLydia);
		b.endedAt = 128.0;
		return b;
	}

	void TestRescue()
	{
		const auto b = Rescue();
		const NarrativeOptions opt;

		const auto lydia = SummaryText(BuildSummary(b, kLydia, "Lydia", opt));
		Check(Has(lydia, "Lydia was struck down by Bandit Chief"), "Lydia is told she was struck down, and by whom", lydia);
		Check(Has(lydia, "Bandit Chief, who had struck down Lydia, was then killed by Kaira."), "the chief's death is credited to Kaira", lydia);
		Check(Has(lydia, "Kaira healed Lydia with Healing Hands"), "the healing is credited to Kaira, with the spell", lydia);
		Check(Has(lydia, "Kaira saved Lydia's life in this battle"), "the plain verdict", lydia);
		Check(Has(lydia, "Lydia killed 1 of the 4 who died on the other side: Bandit."), "Lydia's own kill count", lydia);
		Check(Has(lydia, "Lydia suffered: burned by fire by Bandit Mage (Flames)."), "hostile effects, once, with caster and spell", lydia);
		Check(Has(lydia, "Who killed the enemies: Kaira 3 (Bandit Chief, Bandit and Bandit Mage); Lydia 1 (Bandit)."), "kill tally, most kills first", lydia);
		Check(Has(lydia, "All 4 enemies were killed."), "outcome", lydia);
		Check(Has(lydia, "Kaira and Lydia fought against Bandit Chief, Bandit x2 and Bandit Mage."), "sides, same names counted", lydia);
		Check(Has(lydia, "Kaira: took only light wounds; was never near death; killed 3 of the other side."), "the player's line among the others", lydia);
		Check(!Has(lydia, "points"), "no raw figures unless asked for", lydia);

		const auto hero = SummaryText(BuildSummary(b, kHero, "Kaira", opt));
		Check(Has(hero, "Kaira was never in serious danger."), "Kaira was not in danger", hero);
		Check(Has(hero, "Kaira saved Lydia's life. Bandit Chief, who had struck down Lydia, was then killed by Kaira."), "Kaira's summary says whose life he saved", hero);
		Check(Has(hero, "Kaira was helped by: shielded by magical armor (own Stoneflesh)."), "a self-cast protection", hero);
		Check(Has(hero, "Lydia: took more damage") || Has(hero, "Lydia: was badly hurt"), "Lydia's line among the others", hero);
		Check(Has(hero, "was saved by Kaira"), "Lydia's line says who saved her", hero);

		const auto witness = BuildSummary(b, 0x1A66B, "Hulda", opt);
		Check(witness.show && !witness.participant && witness.personal.empty() && witness.others.size() == 2, "a witness gets the general lines and both fighters",
			SummaryText(witness));

		const auto summary = BuildMemory(b, opt);
		Check(Has(summary, "but Kaira killed Bandit Chief and Kaira healed Lydia: Kaira saved Lydia's life."), "the summary names the rescue", summary);
		Check(Significant(b, 3, 30.0), "a rescue is worth remembering", summary);

		NarrativeOptions numbers;
		numbers.showNumbers = true;
		const auto withNumbers = SummaryText(BuildSummary(b, kLydia, "Lydia", numbers));
		Check(Has(withNumbers, "(about 280 points)"), "figures when asked for", withNumbers);
	}

	// Near death without bleedout, pulled through alone; an atronach's kill is credited with its master.
	void TestNearDeathAndSummon()
	{
		Battle b;
		b.startedAt = 0;
		b.location = "the Whiterun plains";
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kAtronach, "Flame Atronach", Side::kPlayer, 150, kHero));
		b.Join(Info(kBandit1, "Wolf", Side::kEnemy, 50));
		b.Join(Info(kBandit2, "Wolf", Side::kEnemy, 50));
		b.Damage(kHero, kBandit1, 175, 0.125f, 3);
		b.Damage(kBandit1, kAtronach, 50, 0, 5);
		b.Death(kBandit1, kAtronach, 5);
		b.Heal(kHero, kHero, 120, "Potion of Healing", 6);
		b.Damage(kBandit2, kHero, 20, 0.6f, 8);
		b.endedAt = 30;

		const auto hero = SummaryText(BuildSummary(b, kHero, "Kaira", {}));
		Check(Has(hero, "Kaira was brought to the very edge of death by the first Wolf"), "near death without bleedout", hero);
		Check(Has(hero, "The first Wolf, who had nearly killed Kaira, was then killed by Flame Atronach (commanded by Kaira)."), "the summon's kill, with its master", hero);
		Check(!Has(hero, "saved Kaira's life"), "your own summon's kill is not someone else's rescue", hero);
		Check(Has(hero, "Kaira healed themself with Potion of Healing"), "a potion is a self heal", hero);
		Check(Has(hero, "1 of the 2 enemies were killed; the rest fled, yielded or broke off."), "partial outcome", hero);
		Check(Has(hero, "Flame Atronach (commanded by Kaira) 1 (Wolf)"), "kill tally credits the summon with its master", hero);
	}

	// A guard who only fought the bandits counts with the player's side; a townsman who hit the player counts against.
	void TestOthers()
	{
		Battle b;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kBandit1, "Bandit", Side::kEnemy, 100));
		b.Join(Info(kGuard, "Whiterun Guard", Side::kOther, 200));
		b.Join(Info(kMage, "Angry Farmer", Side::kOther, 80));
		b.Damage(kHero, kBandit1, 5, 0.97f, 1);  // the bandit fights the player; the guard then deals with the bandit
		b.Damage(kBandit1, kGuard, 100, 0, 2);
		b.Death(kBandit1, kGuard, 2);
		b.Damage(kHero, kMage, 10, 0.95f, 3);
		b.endedAt = 10;
		Check(b.EffectiveSide(*b.Find(kGuard)) == Side::kPlayer, "the guard fought on the player's side", "");
		Check(b.EffectiveSide(*b.Find(kMage)) == Side::kEnemy, "the farmer fought against the player", "");
		const auto hero = SummaryText(BuildSummary(b, kHero, "Kaira", {}));
		Check(Has(hero, "Kaira and Whiterun Guard fought against Bandit and Angry Farmer."), "sides with the others folded in", hero);
		Check(Has(hero, "Kaira killed none of the 1 who died on the other side."), "no kills is said outright", hero);

		// a follower who turns up in a later report as an enemy stays a follower; an other who turns hostile becomes an enemy
		b.Join(Info(kHero, "Kaira", Side::kEnemy, 200));
		b.Join(Info(kGuard, "Whiterun Guard", Side::kEnemy, 200));
		Check(b.Find(kHero)->info.side == Side::kPlayer && b.Find(kGuard)->info.side == Side::kEnemy, "side changes on rejoin", "");
	}

	// Seen in play: a camp of uniquely named bandits, a deer that only bolted, a long roll of names.
	void TestBigFightAndBystanders()
	{
		Battle b;
		b.startedAt = 0;
		b.location = "Halted Stream Camp";
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kLydia, "Lydia", Side::kPlayer, 130));
		const char* names[] = { "Enjaadia [bandit]", "Darkhu [Bandit Outlaw]", "Marana [bandit]", "Ahtar [Bandit Outlaw]", "Bakhig [Bandit Outlaw]", "Dunius [Bandit Outlaw]" };
		for (ActorId i = 0; i < 6; ++i) {
			b.Join(Info(0xFF001000 + i, names[i], Side::kEnemy, 100));
			b.Damage(0xFF001000 + i, kHero, 100, 0.0f, 5.0 + i);
			b.Death(0xFF001000 + i, kHero, 5.0 + i);
		}
		b.Join(Info(0x846F6, "Deer", Side::kEnemy, 50));  // "in combat" with the player only because it fled
		b.Damage(kHero, 0xFF001003, 20, 0.9f, 4);
		b.endedAt = 40;

		Check(b.Count(Side::kEnemy) == 6 && !b.Involved(*b.Find(0x846F6)), "a deer that only fled is not an enemy", "");
		const auto me = BuildSummary(b, kHero, "Kaira", {});
		const auto text = SummaryText(me);
		Check(Has(text, "Kaira and Lydia fought against Enjaadia [bandit], Darkhu [Bandit Outlaw], Marana [bandit], Ahtar [Bandit Outlaw] and 2 others."),
			"a long roll of enemies is cut short", text);
		Check(!Has(text, "Deer"), "the deer is not told", text);
		Check(Has(text, "All 6 enemies were killed.") && Has(text, "Who killed the enemies: Kaira 6."), "outcome and tally without the long name list", text);
		Check(Has(text, "Kaira killed 6 of the 6 who died on the other side."), "own kills without the long name list", text);
		Check(me.others.size() == 1 && Has(text, "None of them was seriously hurt or in any real danger."), "an easy fight needs no roll call", text);

		// five names are told in full: "and 1 others" would be no shorter
		Battle c;
		c.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		for (ActorId i = 0; i < 5; ++i) {
			c.Join(Info(0xFF002000 + i, names[i], Side::kEnemy, 100));
			c.Damage(0xFF002000 + i, kHero, 10, 0.9f, 1);
		}
		c.endedAt = 10;
		const auto five = SummaryText(BuildSummary(c, kHero, "Kaira", {}));
		Check(Has(five, "Ahtar [Bandit Outlaw] and Bakhig [Bandit Outlaw]."), "five names are told in full", five);
		Check(!Has(five, "None of them"), "alone on your side: no line about the others", five);
	}

	// Seen in play on a creature-heavy list: predators and prey all around a fight are "in combat", some with the
	// player as their target, and fight each other. Only what fought the party, or fought those who did, is the battle.
	void TestWildlifeAroundAFight()
	{
		constexpr ActorId kMinotaur = 0xFF003001, kElk = 0xFF003002, kCrab = 0xFF003003, kEchatere = 0xFF003004, kHorse = 0xFF003005;
		Battle b;
		b.startedAt = 0;
		b.location = "the wilds of Skyrim";
		auto hero = Info(kHero, "Kaira", Side::kPlayer, 100);
		hero.healthPct = 0.12f;  // still hurt from the last fight
		b.Join(hero);
		b.Join(Info(kLydia, "Jenassa", Side::kPlayer, 200));
		b.Join(Info(kMinotaur, "Minotaur", Side::kEnemy, 300));
		b.Join(Info(kElk, "Elk", Side::kEnemy, 60));          // "in combat" with the player only because it fled
		b.Join(Info(kCrab, "Crab", Side::kEnemy, 20));
		b.Join(Info(kEchatere, "Echatere", Side::kEnemy, 80));
		b.Join(Info(kHorse, "Horse", Side::kOther, 100));
		b.Sample(kHero, 0.12f, 1);
		b.Damage(kMinotaur, kHero, 300, 0.0f, 5);
		b.Death(kMinotaur, kHero, 5);
		b.Damage(kCrab, kEchatere, 20, 0.0f, 6);   // a predator and its prey: nothing to do with the party
		b.Death(kCrab, kEchatere, 6);
		b.Damage(kHorse, kEchatere, 30, 0.7f, 7);
		b.endedAt = 60;

		Check(b.Count(Side::kEnemy) == 1 && b.Dead(Side::kEnemy) == 1, "only what fought the party is an enemy", "");
		Check(!b.Find(kHero)->WasCritical(), "walking in at death's door is not being brought there", "");
		const auto me = BuildSummary(b, kHero, "Kaira", {});
		const auto text = SummaryText(me);
		Check(Has(SummaryText(BuildSummary([] {
			Battle o;
			o.Join(Info(kLydia, "Jenassa", Side::kPlayer, 200));  // the follower joined first
			o.Join(Info(kHero, "Kaira", Side::kPlayer, 100));
			o.Join(Info(kBandit1, "Bandit", Side::kEnemy, 50));
			o.Damage(kBandit1, kLydia, 10, 0.8f, 1);
			o.endedAt = 5;
			return o;
		}(), kHero, "Kaira", {})), "Kaira and Jenassa fought against Bandit."), "the player leads the list, whoever joined first", "");
		Check(Has(text, "Kaira and Jenassa fought against Minotaur.") && Has(text, "The one enemy was killed.") && Has(text, "Who killed the enemies: Kaira 1 (Minotaur)."),
			"the battle is the party against the minotaur", text);
		Check(!Has(text, "Echatere") && !Has(text, "Crab") && !Has(text, "Elk") && !Has(text, "Horse") && !Has(text, "caught up"), "the wildlife's own quarrels are not told", text);
		Check(Has(text, "Kaira went into this fight already close to death from earlier wounds") && !Has(text, "edge of death"), "already wounded going in", text);
		const auto elk = BuildSummary(b, kElk, "Elk", {});
		Check(!elk.participant && elk.personal.empty(), "what never fought is a witness, not a fighter", SummaryText(elk));
		const auto other = SummaryText(BuildSummary(b, kLydia, "Jenassa", {}));
		Check(Has(other, "Kaira: took no damage worth the name; went in already close to death from earlier wounds; killed 1 of the other side."), "the companion's line", other);

		// someone who fights an enemy of the party is part of it; hurt further while already low counts as near death
		Battle c;
		c.Join(hero);
		c.Join(Info(kMinotaur, "Minotaur", Side::kEnemy, 300));
		c.Join(Info(kGuard, "Whiterun Guard", Side::kOther, 200));
		c.Damage(kHero, kMinotaur, 8, 0.04f, 1);
		c.Damage(kMinotaur, kGuard, 50, 0.8f, 2);
		c.endedAt = 10;
		Check(c.Involved(*c.Find(kGuard)) && c.EffectiveSide(*c.Find(kGuard)) == Side::kPlayer, "a guard who fights the party's enemy is on the party's side", "");
		Check(c.Find(kHero)->WasCritical() && c.Find(kHero)->broughtLowBy == kMinotaur, "hurt further while already low is near death", "");
	}

	// Reported by a player: "NPCs think I killed everyone myself, even if I am just healing". The game's death report
	// named the player for kills the followers made.
	void TestKillCreditFollowsTheFinalBlow()
	{
		Battle b;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
		b.Join(Info(kBandit1, "Cave Bear", Side::kEnemy, 300));
		b.Join(Info(kBandit2, "Cave Bear", Side::kEnemy, 300));
		b.Join(Info(kMage, "Cultist", Side::kEnemy, 100));
		b.Damage(kLydia, kBandit1, 60, 0.8f, 1);
		b.Heal(kLydia, kHero, 60, "Healing Hands", 2);
		b.Damage(kBandit1, kLydia, 300, 0.0f, 3);
		b.Death(kBandit1, kHero, 3.1);          // the game says the player did it
		b.Damage(kBandit2, kLydia, 300, 0.0f, 5);
		b.Death(kBandit2, kHero, 5.1);
		b.Damage(kMage, kLydia, 40, 0.6f, 6);
		b.Death(kMage, kHero, 20);              // no blow near the death, and the one the game names never struck them
		b.Join(Info(kChief, "Giant", Side::kEnemy, 600));
		b.Damage(kChief, kHero, 30, 0.95f, 21);
		b.Death(kChief, kHero, 30);             // no blow near the death, but the one named did strike them: the game's word stands
		b.Join(Info(kGuard, "Uthgerd", Side::kPlayer, 180));
		b.Damage(kGuard, kChief, 180, 0.0f, 22);
		b.Death(kGuard, kHero, 26);             // felled by the giant, reported late and against the player
		b.endedAt = 31;
		Check(b.Find(kLydia)->kills.size() == 3 && b.Find(kHero)->kills.size() == 1, "the one who dealt the final blow made the kill", "");
		const auto hero = SummaryText(BuildSummary(b, kHero, "Kaira", {}));
		Check(Has(hero, "Who killed the enemies: Lydia 3 (Cave Bear x2 and Cultist); Kaira 1 (Giant)."), "tally by final blow", hero);
		Check(Has(hero, "Uthgerd was killed by Giant.") && !Has(hero, "by accident"), "nobody is accused of killing an ally they never struck", hero);
		Check(Has(hero, "Kaira took no damage worth the name and dealt only a small part of their side's damage."), "a healer is not told as the one who did the fighting", hero);
	}

	// Injury mods (Blade and Blunt, Wildcat) and dismemberment (Dismembering Framework, the game's own beheading).
	void TestInjuriesAndDismemberment()
	{
		Battle b;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
		b.Join(Info(kChief, "Bandit Chief", Side::kEnemy, 400));
		b.Join(Info(kBandit1, "Bandit", Side::kEnemy, 100));
		b.Join(Info(kBandit2, "Bandit", Side::kEnemy, 100));
		b.Injury(kHero, "Minor Injury", true, 0);  // had it before the fight
		b.Damage(kLydia, kChief, 60, 0.8f, 2);
		b.Injury(kLydia, "Leg Injury", false, 3);
		b.Injury(kLydia, "Leg Injury", false, 4);  // seen again: told once
		b.Damage(kHero, kBandit1, 20, 0.9f, 5);
		b.Injury(kHero, "Major Injury", false, 30);  // long after the last blow: nobody named
		b.Damage(kChief, kLydia, 400, 0.0f, 31);
		b.Death(kChief, kLydia, 31);
		b.Dismember(kChief, true);
		b.Damage(kBandit1, kHero, 100, 0.0f, 32);
		b.Death(kBandit1, kHero, 32);
		b.Dismember(kBandit1, false);
		b.Damage(kBandit2, kHero, 100, 0.0f, 33);
		b.Death(kBandit2, kHero, 33);
		b.Dismember(kBandit2, false);
		b.Dismember(kLydia, true);  // the living are not dismembered
		b.endedAt = 40;

		const auto lydia = SummaryText(BuildSummary(b, kLydia, "Lydia", {}));
		Check(Has(lydia, "Lydia was injured in this fight and still carries it: Leg Injury (dealt by Bandit Chief)."), "a new injury, once, with its dealer", lydia);
		Check(Has(lydia, "Lydia beheaded Bandit Chief."), "a beheading is credited to the killer", lydia);
		Check(Has(lydia, "Kaira dismembered Bandit x2, severing a limb."), "severed limbs, grouped per killer", lydia);
		Check(Has(lydia, "Kaira: took only light wounds; was never near death; was injured (Major Injury); killed 2 of the other side."), "a companion's injury", lydia);
		Check(!b.Find(kLydia)->dismembered, "only the dead are dismembered", lydia);

		const auto hero = SummaryText(BuildSummary(b, kHero, "Kaira", {}));
		Check(Has(hero, "Kaira went into this fight already carrying: Minor Injury."), "an injury brought into the fight", hero);
		Check(Has(hero, "Kaira was injured in this fight and still carries it: Major Injury."), "an injury with no blow near it names nobody", hero);
		Check(!Has(hero, "None of them was seriously hurt"), "an injury makes it more than an easy fight", hero);

		const auto memory = BuildMemory(b, {});
		Check(Has(memory, "Lydia came out of it injured: Leg Injury (dealt by Bandit Chief).") && Has(memory, "Lydia beheaded Bandit Chief."), "both are remembered", memory);
		Check(Significant(b, 9, 900.0), "an injury is worth remembering", memory);
	}

	// Asked for by the author after a brawl in Whiterun in which a preacher was cut down and counted as an enemy.
	void TestInnocentsAndAllies()
	{
		Battle b;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
		b.Join(Info(kGuard, "Whiterun Guard", Side::kEnemy, 250));
		b.Join(Info(kBandit1, "Heimskr", Side::kEnemy, 75));  // the game calls him hostile once he has been struck
		b.Join(Info(kBandit2, "Whiterun Guard", Side::kEnemy, 250));
		b.Join(Info(kMage, "Bandit", Side::kEnemy, 100));
		for (const auto id : { kGuard, kBandit1, kBandit2 }) b.Find(id)->info.lawful = true;
		b.Armed(kGuard);
		b.Damage(kLydia, kGuard, 30, 0.9f, 1);
		b.Damage(kGuard, kHero, 250, 0.0f, 2);
		b.Death(kGuard, kHero, 2);
		b.Damage(kBandit1, kHero, 75, 0.0f, 3);
		b.Death(kBandit1, kHero, 3);
		b.Armed(kBandit2);  // swung and missed: a fighter all the same
		// a hostile who had turned on the party but was cut down before his sword was out (reported by a player: a
		// Dawnguard told as an innocent)
		b.Join(Info(kAtronach, "Dawnguard", Side::kEnemy, 150));
		b.Find(kAtronach)->info.lawful = true;
		b.CameToFight(kAtronach);
		b.Damage(kAtronach, kHero, 150, 0.0f, 3.5);
		b.Death(kAtronach, kHero, 3.5);
		b.Damage(kBandit2, kLydia, 250, 0.0f, 4);
		b.Death(kBandit2, kLydia, 4);
		b.Damage(kMage, kHero, 100, 0.0f, 5);  // never drew, but no hold's law covers a bandit
		b.Death(kMage, kHero, 5);
		b.Damage(kLydia, kHero, 300, 0.0f, 6);
		b.Death(kLydia, kHero, 6);
		b.endedAt = 10;

		const auto hero = SummaryText(BuildSummary(b, kHero, "Kaira", {}));
		Check(Has(hero, "Heimskr, who was not actively engaged in combat, was killed by Kaira."), "the death of someone who was not fighting is said outright", hero);
		Check(Has(hero, "Kaira and Lydia fought against Whiterun Guard x2, Bandit and Dawnguard.") && Has(hero, "All 4 enemies were killed.") && !Has(hero, "Dawnguard, who was not"), "someone who was not fighting is not one of the enemies", hero);
		Check(Has(hero, "Also caught up in it: Heimskr."), "but was caught up in it", hero);
		Check(Has(hero, "Lydia was killed by Kaira, by accident: they were on the same side."), "an ally killed by their own side", hero);
		Check(Has(hero, "Kaira killed 3 of the 4 who died on the other side"), "and is not one of the killer's enemy kills", hero);

		Battle m;  // nothing but a murder is still something to tell
		m.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		m.Join(Info(kBandit1, "Heimskr", Side::kOther, 75));
		m.Find(kBandit1)->info.lawful = true;
		m.Damage(kBandit1, kHero, 75, 0.0f, 1);
		m.Death(kBandit1, kHero, 1);
		m.endedAt = 2;
		Check(Significant(m, 3, 30.0) && Has(BuildMemory(m, {}), "Heimskr, who was not actively engaged in combat, was killed by Kaira."), "a murder is remembered", BuildMemory(m, {}));
	}

	// Seen in a brawl of spawned monsters in Windhelm: a monster that killed the one that had downed a townsman was
	// told as having saved his life, and a death at the hands of something the battle never saw as "killed by someone".
	void TestFoesSaveNobody()
	{
		Battle b;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
		b.Join(Info(kGuard, "Ursine", Side::kPlayer, 100));
		b.Join(Info(kChief, "Dwarven Centurion", Side::kEnemy, 400));
		b.Join(Info(kMage, "Sinmur", Side::kEnemy, 900));
		b.Damage(kHero, kMage, 10, 0.95f, 1);
		b.Damage(kLydia, kChief, 290, 0.03f, 2);
		b.Down(kLydia, 2);
		b.Damage(kChief, kMage, 400, 0.0f, 5);
		b.Death(kChief, kMage, 5);
		b.Death(kGuard, 0xABCDEF, 6);  // killed by something that never joined
		b.endedAt = 10;
		const auto lydia = SummaryText(BuildSummary(b, kLydia, "Lydia", {}));
		Check(Has(lydia, "Dwarven Centurion, who had struck down Lydia, was then killed by Sinmur.") && !Has(lydia, "saved Lydia"), "a foe's kill is told, but is no rescue", lydia);
		Check(Has(lydia, "Ursine was killed, by whom is not known.") && !Has(lydia, "someone"), "an unseen killer is not 'someone'", lydia);
		Check(Has(BuildMemory(b, {}), "Lydia was struck down by Dwarven Centurion: collapsed, helpless and bleeding out, unable to fight, but survived."), "and it is remembered so", BuildMemory(b, {}));
	}

	// Asked for by a player whose followers kept complaining of being "drained and roasted" by the player's area spells.
	void TestFriendlyFire()
	{
		Battle b;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
		b.Join(Info(kBandit1, "Bandit", Side::kEnemy, 100));
		b.Damage(kLydia, kBandit1, 30, 0.9f, 1);
		b.Effect(kLydia, kBandit1, "poisoned", "Weak Poison", true);
		b.Effect(kLydia, kHero, "burned by fire", "Fireball", true);
		b.Damage(kLydia, kHero, 240, 0.1f, 2);  // the fireball takes her to the brink
		b.Damage(kBandit1, kHero, 100, 0.0f, 3);
		b.Death(kBandit1, kHero, 3);
		b.endedAt = 10;

		const auto told = SummaryText(BuildSummary(b, kLydia, "Lydia", {}));
		Check(Has(told, "Lydia suffered: poisoned by Bandit (Weak Poison).") && Has(told, "Lydia was caught in their own side's attacks, by accident (friendly fire, not an attack on them): took part of their damage from Kaira; burned by fire by Kaira (Fireball)."),
			"friendly fire is told apart, as an accident", told);
		Check(Has(told, "brought to the very edge of death by Kaira (by accident: friendly fire)"), "brought low by an ally: an accident", told);
		Check(Has(told, "Lydia was badly hurt") && !Has(told, "Part of the damage"), "damage from an ally is named as an accident", told);
		NarrativeOptions numbers;
		numbers.showNumbers = true;
		const auto hero = SummaryText(BuildSummary(b, kHero, "Kaira", numbers));
		Check(Has(hero, "dealt most of their side's damage (about 100 points)."), "what was done to an ally is not counted as fighting", hero);

		NarrativeOptions quiet;
		quiet.ignoreFriendlyFire = true;
		const auto hidden = SummaryText(BuildSummary(b, kLydia, "Lydia", quiet));
		Check(!Has(hidden, "Fireball") && !Has(hidden, "friendly fire") && !Has(hidden, "by Kaira"), "friendly fire left out", hidden);
		Check(Has(hidden, "Lydia took only light wounds") && !Has(hidden, "Part of the damage"), "and their damage is no part of the wounds told", hidden);
		Check(Has(hidden, "Lydia suffered: poisoned by Bandit (Weak Poison).") && Has(hidden, "Lydia was brought to the very edge of death, with almost no health left."), "the rest still told", hidden);
		Check(!Has(BuildMemory(b, quiet), "Kaira (by accident"), "and left out of what is remembered", BuildMemory(b, quiet));
	}

	// Reported by a player: "Ignore friendly fire" worked for followers but not for anyone else, and a bystander caught
	// in the crossfire of a fight with bandits was told as one of the enemies.
	void TestFriendlyFireOnBystanders()
	{
		Battle b;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
		b.Join(Info(kBandit1, "Bandit", Side::kEnemy, 100));
		b.Join(Info(kChief, "Bandit Chief", Side::kEnemy, 300));
		b.Join(Info(kGuard, "Whiterun Guard", Side::kOther, 250));
		b.Join(Info(kMage, "Hunter", Side::kOther, 100));        // stood by: no hold's law, never fought
		b.Join(Info(kAtronach, "Heimskr", Side::kOther, 75));
		b.Find(kGuard)->info.lawful = b.Find(kAtronach)->info.lawful = true;
		b.Damage(kLydia, kBandit1, 30, 0.9f, 1);
		b.Damage(kChief, kGuard, 60, 0.8f, 1.5);                 // the guard fights the bandits...
		b.Effect(kGuard, kHero, "burned by fire", "Fireball", true, 2);
		b.Damage(kGuard, kHero, 50, 0.8f, 2);                     // ...and is caught in the player's fireball
		b.Effect(kMage, kHero, Text::Get("fx.drained_life"), "", true, 2, Damaging(true));
		b.Damage(kMage, kHero, 40, 0.6f, 2);                      // so is the hunter, who fought nobody
		b.Effect(kLydia, kHero, "burned by fire", "Fireball", true, 2);
		b.Damage(kLydia, kHero, 40, 0.75f, 2);
		b.Effect(kBandit1, kHero, "burned by fire", "Fireball", true, 2);
		b.Damage(kBandit1, kHero, 100, 0.0f, 2);
		b.Death(kBandit1, kHero, 2);
		b.Damage(kAtronach, kHero, 75, 0.0f, 2.2);
		b.Death(kAtronach, kHero, 2.2);
		b.Damage(kChief, kLydia, 300, 0.0f, 6);
		b.Death(kChief, kLydia, 6);
		b.endedAt = 10;

		Check(b.EffectiveSide(*b.Find(kMage)) == Side::kOther, "a bystander struck only by the player's side is not an enemy", "");
		Check(b.EffectiveSide(*b.Find(kGuard)) == Side::kPlayer, "a guard who fought the bandits is on the player's side", "");
		const auto hero = SummaryText(BuildSummary(b, kHero, "Kaira", {}));
		Check(Has(hero, "Kaira, Lydia and Whiterun Guard fought against Bandit and Bandit Chief.") && Has(hero, "Also caught up in it: Hunter and Heimskr."),
			"the bystanders are caught up in it, not enemies", hero);

		NarrativeOptions quiet;
		quiet.ignoreFriendlyFire = true;
		const auto hunterTold = SummaryText(BuildSummary(b, kMage, "Hunter", {}));
		Check(Has(hunterTold, "Hunter was caught in the crossfire, by accident (not an attack on them): took part of their damage from Kaira; drained of life by Kaira.")
				&& !Has(hunterTold, "own side") && !Has(hunterTold, "Hunter suffered"),
			"a bystander hit by the player's side: told as an accident", hunterTold);
		const auto hunterHidden = SummaryText(BuildSummary(b, kMage, "Hunter", quiet));
		Check(!Has(hunterHidden, "from Kaira") && !Has(hunterHidden, "drained") && !Has(hunterHidden, "friendly fire") && Has(hunterHidden, "Hunter took no damage worth the name"),
			"a bystander hit by the player's side: left out", hunterHidden);

		const auto bandit = SummaryText(BuildSummary(b, kBandit1, "Bandit", quiet));
		Check(Has(bandit, "Bandit suffered: burned by fire by Kaira (Fireball).") && Has(bandit, "Bandit was killed by Kaira."), "what the player did to an enemy is still told", bandit);

		const auto guardTold = SummaryText(BuildSummary(b, kGuard, "Whiterun Guard", {}));
		Check(Has(guardTold, "Whiterun Guard was caught in their own side's attacks, by accident (friendly fire, not an attack on them): took part of their damage from Kaira; burned by fire by Kaira (Fireball)."),
			"a guard who fought beside the party: friendly fire, as before", guardTold);
		const auto guardHidden = SummaryText(BuildSummary(b, kGuard, "Whiterun Guard", quiet));
		Check(!Has(guardHidden, "Fireball") && !Has(guardHidden, "from Kaira"), "and left out as before", guardHidden);

		const auto lydiaTold = SummaryText(BuildSummary(b, kLydia, "Lydia", {}));
		const auto lydiaHidden = SummaryText(BuildSummary(b, kLydia, "Lydia", quiet));
		Check(Has(lydiaTold, "Lydia was caught in their own side's attacks") && !Has(lydiaHidden, "Fireball"), "a follower: unchanged", lydiaTold + lydiaHidden);

		// a death by friendly fire is always told
		Check(Has(SummaryText(BuildSummary(b, kAtronach, "Heimskr", quiet)), "Heimskr was killed by Kaira.")
				&& Has(BuildMemory(b, quiet), "Heimskr, who was not actively engaged in combat, was killed by Kaira."),
			"a bystander's death is told, the option or not", BuildMemory(b, quiet));

		// one only the player's enemies went for is still counted with the player's side
		Battle c;
		c.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		c.Join(Info(kBandit1, "Bandit", Side::kEnemy, 100));
		c.Join(Info(kMage, "Hunter", Side::kOther, 100));
		c.Damage(kHero, kBandit1, 10, 0.95f, 1);
		c.Damage(kMage, kBandit1, 30, 0.7f, 2);
		Check(c.EffectiveSide(*c.Find(kMage)) == Side::kPlayer, "struck by the player's enemies: with the player's side", "");

		// with no enemies about, whoever the party went for is no bystander: a deer hunt is still a fight with the deer...
		Battle d;
		d.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		d.Join(Info(0x846F6, "Deer", Side::kOther, 50));
		d.Damage(0x846F6, kHero, 50, 0.0f, 1);
		d.Death(0x846F6, kHero, 1);
		d.endedAt = 5;
		const auto hunt = SummaryText(BuildSummary(d, kHero, "Kaira", quiet));
		Check(d.EffectiveSide(*d.Find(0x846F6)) == Side::kEnemy && d.Count(Side::kEnemy) == 1 && Has(hunt, "Kaira fought against Deer.") && Has(hunt, "Who killed the enemies: Kaira 1 (Deer)."),
			"a deer hunt is a battle with the deer, as before", hunt);

		// ...and a townsman attacked on his own is told as attacked, the option or not
		Battle t;
		t.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		t.Join(Info(kAtronach, "Heimskr", Side::kOther, 75));
		t.Join(Info(kMage, "Farmhand", Side::kOther, 80));
		t.Find(kAtronach)->info.lawful = true;
		t.Effect(kAtronach, kHero, "burned by fire", "Flames", true, 1);
		t.Damage(kAtronach, kHero, 40, 0.45f, 1);
		t.Damage(kMage, kHero, 30, 0.6f, 2);  // no hold's law
		t.endedAt = 5;
		Check(t.EffectiveSide(*t.Find(kMage)) == Side::kEnemy, "someone else attacked on their own is the one the player fought, as before", "");
		for (const bool ignore : { false, true }) {
			NarrativeOptions o;
			o.ignoreFriendlyFire = ignore;
			const auto assault = SummaryText(BuildSummary(t, kAtronach, "Heimskr", o));
			Check(Has(assault, "Heimskr suffered: burned by fire by Kaira (Flames).") && !Has(assault, "by accident") && !Has(assault, "crossfire") && Has(assault, "took solid wounds"),
				"a lone assault on a townsman is told as an attack", assault);
		}
	}

	// Seen in a player's log: something on the player pulsed an Absorb Health effect onto everyone nearby every half
	// second, damaging none of them, and each was told the player had "drained their life". A damage effect is told
	// only when it took health; what does no damage (paralysis, a slow from a spell without damage) is told as before.
	void TestHarmlessEffects()
	{
		constexpr std::uint32_t kDrainSpell = 0xFF00A001, kFrostbite = 0xFF00A002, kParalyze = 0xFF00A003;
		const auto drained = Text::Get("fx.drained_life"), frozen = Text::Get("fx.frozen"), slowed = Text::Get("fx.slowed"), paralyzed = Text::Get("fx.paralyzed");
		const auto replay = [&](bool a_ignoreHarmless) {
			Battle b;
			b.startedAt = 0;
			b.ignoreHarmless = a_ignoreHarmless;
			b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
			b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
			b.Join(Info(kBandit1, "Bandit", Side::kEnemy, 100));
			b.Join(Info(kMage, "Bandit Mage", Side::kEnemy, 80));
			b.Join(Info(kGuard, "Hunter", Side::kOther, 100));
			for (double t = 1.0; t <= 6.0; t += 0.5) {  // the pulse, on everyone in reach, doing nothing to any of them
				for (const auto id : { kLydia, kBandit1, kGuard }) b.Effect(id, kHero, drained, "", true, t, Damaging(false));
			}
			b.Damage(kBandit1, kHero, 30, 0.7f, 2.0);  // the player's sword, while the pulse was on him
			b.Effect(kLydia, kMage, drained, "Vampiric Drain", true, 3.0, Damaging(true, kDrainSpell));
			b.Damage(kLydia, kMage, 20, 0.9f, 3.2);  // what it took
			b.Effect(kHero, kMage, frozen, "Frostbite", true, 4.0, Damaging(true, kFrostbite, 1.0));
			b.Effect(kHero, kMage, slowed, "Frostbite", true, 4.0, { kFrostbite });
			b.Damage(kHero, kMage, 15, 0.9f, 4.6);
			b.Effect(kLydia, kBandit1, paralyzed, "Paralyze", true, 5.0, { kParalyze });  // no damage in that spell
			b.Effect(kLydia, kMage, frozen, "Frostbite", true, 6.0, Damaging(false, kFrostbite, 1.0));  // a frostbite that did nothing...
			b.Effect(kLydia, kMage, slowed, "Frostbite", true, 6.0, { kFrostbite });                    // ...and its slow
			b.Damage(kBandit1, kLydia, 70, 0.0f, 7);
			b.Death(kBandit1, kLydia, 7);
			b.endedAt = 10;
			return b;
		};

		const auto b = replay(true);
		Check(!b.Involved(*b.Find(kGuard)), "a damage effect that took nothing draws nobody into the battle", "");
		const auto hero = SummaryText(BuildSummary(b, kHero, "Kaira", {}));
		Check(!Has(hero, "Hunter"), "and they are not told as caught up in it", hero);
		Check(Has(hero, "Kaira suffered: frozen by frost by Bandit Mage (Frostbite) and slowed by Bandit Mage (Frostbite)."), "a frost spell that took health: its slow goes with it", hero);
		const auto lydia = SummaryText(BuildSummary(b, kLydia, "Lydia", {}));
		Check(Has(lydia, "Lydia suffered: drained of life by Bandit Mage (Vampiric Drain) and paralyzed by Bandit (Paralyze)."), "a drain that took health is told; paralysis without damage too", lydia);
		Check(!Has(lydia, "frozen") && !Has(lydia, "slowed") && !Has(lydia, "by Kaira") && !Has(lydia, "caught in"),
			"a frost spell that did nothing, its slow, and an empty drain from her own side are not", lydia);
		const auto bandit = SummaryText(BuildSummary(b, kBandit1, "Bandit", {}));
		Check(!Has(bandit, "drained"), "an empty drain on an enemy the player struck is not told", bandit);
		const auto hunter = BuildSummary(b, kGuard, "Hunter", {});
		Check(!hunter.participant && !Has(SummaryText(hunter), "drained"), "a bystander it reached is told nothing about it", SummaryText(hunter));

		const auto all = replay(false);  // as 1.3.0: every damage effect told when it lands
		Check(Has(SummaryText(BuildSummary(all, kBandit1, "Bandit", {})), "Bandit suffered: drained of life by Kaira."), "with the setting off, the drains are told", "");
		Check(all.Involved(*all.Find(kGuard)) && Has(SummaryText(BuildSummary(all, kGuard, "Hunter", {})), "drained of life by Kaira"), "on bystanders too", "");
		Check(Has(SummaryText(BuildSummary(all, kLydia, "Lydia", {})), "frozen by frost by Bandit Mage (Frostbite)"), "and the frost spell that did nothing", "");
	}

	// The first of the "significant moments": several foes killed by one fighter in the same instant.
	void TestKilledAtOnce()
	{
		Battle b;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
		b.Join(Info(kBandit1, "Bandit", Side::kEnemy, 100));
		b.Join(Info(kBandit2, "Bandit", Side::kEnemy, 100));
		b.Join(Info(kChief, "Bandit Chief", Side::kEnemy, 300));
		b.Join(Info(kMage, "Bandit Mage", Side::kEnemy, 80));
		b.Join(Info(kGuard, "Bandit Archer", Side::kEnemy, 80));
		b.Damage(kHero, kChief, 20, 0.9f, 1);
		// a fireball: three die within the same half second
		for (const auto id : { kBandit1, kBandit2, kChief }) {
			b.Effect(id, kHero, "burned by fire", "Fireball", true, 4.9);
			b.Damage(id, kHero, 400, 0.0f, 5.0);
		}
		b.Death(kBandit1, kHero, 5.0);
		b.Death(kBandit2, kHero, 5.1);
		b.Death(kChief, kHero, 5.4);
		for (const auto id : { kBandit1, kBandit2, kChief }) b.Find(id)->killedWith = b.RecentSpell(id, kHero, 5.4, 1.5);
		// Lydia's two kills are seconds apart: not at once
		b.Damage(kMage, kLydia, 80, 0.0f, 8);
		b.Death(kMage, kLydia, 8);
		b.Damage(kGuard, kLydia, 80, 0.0f, 11);
		b.Death(kGuard, kLydia, 11);
		b.endedAt = 15;
		const auto told = SummaryText(BuildSummary(b, kLydia, "Lydia", {}));
		Check(Has(told, "Kaira killed 3 at once with Fireball: Bandit x2 and Bandit Chief."), "three in one stroke, and with what", told);
		Check(!Has(told, "Lydia killed 2 at once"), "kills seconds apart are not at once", told);
		b.Find(kChief)->killedWith = "Steel Greatsword";  // not all by the same means: the means is not told
		Check(Has(SummaryText(BuildSummary(b, kLydia, "Lydia", {})), "Kaira killed 3 at once: Bandit x2 and Bandit Chief."), "mixed means are left unsaid", told);
		b.Find(kChief)->killedWith = "Fireball";
		Check(Has(BuildMemory(b, {}), "Kaira killed 3 at once with Fireball: Bandit x2 and Bandit Chief."), "and it is remembered", BuildMemory(b, {}));
	}

	// Every phrase can be reworded by the player; nothing they type may blank a summary.
	void TestOwnWording()
	{
		std::set<std::string_view> keys;
		bool                       tidy = true;
		for (const auto& e : Text::Defaults()) {
			tidy = tidy && keys.insert(e.key).second && !e.text.empty() && e.text.front() != ' ' && e.text.back() != ' ' && Text::Accepts(e.key, e.text);
		}
		Check(tidy && keys.size() > 140, "every phrase has its own key and survives an ini file (no spaces at either end)", std::to_string(keys.size()));

		std::string why;
		Check(!Text::Accepts("no.such.phrase", "anything", &why) && why == "there is no phrase of that name", "a line for no phrase is refused", why);
		Check(!Text::Accepts("killed.not_fighting", "{name}, an innocent, was murdered by {murderer}.", &why) && Has(why, "{murderer}"), "a placeholder the phrase lacks is refused", why);
		Check(Text::Accepts("killed.not_fighting", "An innocent died."), "a placeholder may be left out", "");

		Battle m;
		m.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		m.Join(Info(kBandit1, "Heimskr", Side::kOther, 75));
		m.Find(kBandit1)->info.lawful = true;
		m.Damage(kBandit1, kHero, 75, 0.0f, 1);
		m.Death(kBandit1, kHero, 1);
		m.endedAt = 2;
		const auto before = BuildMemory(m, {});

		const auto taken = Text::SetOverrides({
			{ "killed.not_fighting", "{name}, an innocent, {killed}." },
			{ "killed.by", "was cut down by {killer}" },
			{ "battle.fought", "Blood was spilled{at_place}." },            // {duration} left out
			{ "wounds.light", "{name} barely bled" },                        // refused: no {name} in that phrase
			{ "no.such.phrase", "ignored" },
			{ "dealt.none", "" },                                            // empty: the built-in stands
		});
		const auto own = BuildMemory(m, {});
		Check(taken == 3, "the lines that can be used are taken, the rest left out", std::to_string(taken));
		Check(Has(own, "Blood was spilled.") && Has(own, "Heimskr, an innocent, was cut down by Kaira."), "the player's wording is told", own);
		Check(Text::Get("wounds.light") == "took only light wounds" && Text::Get("dealt.none") == "dealt no damage to anyone",
			"a refused or empty line falls back to the built-in wording", Text::Get("wounds.light") + " / " + Text::Get("dealt.none"));

		Text::SetOverrides({});
		Check(BuildMemory(m, {}) == before, "and without the file the built-in wording is back", BuildMemory(m, {}));

		const auto ref = Text::Reference();
		Check(Has(ref, "\n[Deaths]\n") && Has(ref, "killed.not_fighting = {name}, who was not actively engaged in combat, {killed}.") && Has(ref, "fx.burned = burned by fire"),
			"the reference file lists every phrase under its heading", ref.substr(0, 400));
	}

	// Seen in play: three wolves of one name each left the same effect, told three times over; and a bite read as
	// "harmful magic" under a name never meant to be read.
	void TestSayLessWhenInDoubt()
	{
		Battle b;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		for (const auto id : { kBandit1, kBandit2, kChief }) {
			b.Join(Info(id, "Caged Wolf", Side::kEnemy, 60));
			b.Damage(kHero, id, 10, 0.9f, 1);
			b.Effect(kHero, id, Text::Get("fx.hurt"), "", true, 1);  // the tracker leaves an unknowable source unnamed
		}
		b.Effect(kHero, kBandit1, "burned by fire", "Flames", true, 2);
		b.endedAt = 5;
		const auto told = SummaryText(BuildSummary(b, kHero, "Kaira", {}));
		Check(Has(told, "Kaira suffered: hurt by a harmful effect by Caged Wolf and burned by fire by Caged Wolf (Flames)."), "the same thing from same-named attackers is told once, and nothing is guessed", told);
	}

	// A player's report: any line about someone killed without fighting set their followers (and other mods tracking
	// such things) against them. Left out by default; enemies killed unaware still count.
	void TestNonCombatantsLeftOut()
	{
		constexpr ActorId kDeer = 0xFF000901, kVigilant = 0xFF000902;
		const auto fight = [&](bool a_omit) {
			Battle b;
			b.omitBystanders = a_omit;
			b.startedAt = 0;
			b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
			b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
			b.Join(Info(kBandit1, "Bandit", Side::kEnemy, 100));
			b.Join(Info(kBandit2, "Heimskr", Side::kOther, 75));
			b.Find(kBandit2)->info.lawful = true;
			b.Join(Info(kDeer, "Deer", Side::kOther, 40));
			b.Join(Info(kVigilant, "Vigilant of Stendarr", Side::kOther, 150));
			b.Find(kVigilant)->info.lawful = true;
			b.Find(kVigilant)->info.wouldFight = true;  // his order hunts vampires: an enemy, asleep or not
			b.Damage(kLydia, kBandit1, 30, 0.9f, 1);
			b.Damage(kBandit1, kLydia, 100, 0.0f, 2);
			b.Death(kBandit1, kLydia, 2);
			b.Damage(kBandit2, kHero, 75, 0.0f, 3);  // a townsman cut down in the fight
			b.Death(kBandit2, kHero, 3);
			b.Damage(kDeer, kHero, 40, 0.0f, 4);
			b.Death(kDeer, kHero, 4);
			b.Damage(kVigilant, kHero, 150, 0.0f, 5);  // a sneak attack: he never drew
			b.Death(kVigilant, kHero, 5);
			b.endedAt = 10;
			return b;
		};
		const auto left = fight(true);
		const auto told = SummaryText(BuildSummary(left, kLydia, "Lydia", {}));
		Check(!Has(told, "Heimskr") && !Has(told, "Deer") && !Has(told, "caught up"), "non-combatants are left out altogether", told);
		Check(Has(told, "Kaira and Lydia fought against Bandit and Vigilant of Stendarr.") && Has(told, "All 2 enemies were killed."),
			"an enemy killed unaware still counts", told);
		Check(Has(told, "Who killed the enemies: Lydia 1 (Bandit); Kaira 1 (Vigilant of Stendarr)."), "and is credited", told);
		Check(!Has(BuildMemory(left, {}), "Heimskr"), "and nothing of them is remembered", BuildMemory(left, {}));

		const auto mentioned = SummaryText(BuildSummary(fight(false), kLydia, "Lydia", {}));
		Check(Has(mentioned, "Heimskr, who was not actively engaged in combat, was killed by Kaira."), "with the setting on, the old line", mentioned);
		Check(!Has(mentioned, "Vigilant of Stendarr, who was not"), "and an enemy killed unaware is never one of them", mentioned);

		Battle only;  // a townsman's murder and nothing else: no battle to tell, with them left out
		only.omitBystanders = true;
		only.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		only.Join(Info(kBandit2, "Heimskr", Side::kOther, 75));
		only.Find(kBandit2)->info.lawful = true;
		only.Damage(kBandit2, kHero, 75, 0.0f, 1);
		only.Death(kBandit2, kHero, 1);
		only.endedAt = 2;
		Check(only.Count(Side::kEnemy) == 0 && !Significant(only, 3, 30.0), "a murder alone is no battle to remember", BuildMemory(only, {}));
	}

	// Reported by players: a companion's mechanical spider, re-summoned all through a long fight, was told as killed 18
	// times; followers mourned a conjured skeleton.
	void TestSummons()
	{
		constexpr ActorId kRemiel = 0xFF000911, kScrap1 = 0xFF000912, kScrap2 = 0xFF000913, kScrap3 = 0xFF000914, kWolf1 = 0xFF000915, kWolf2 = 0xFF000916;
		Battle b;
		b.omitBystanders = true;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kRemiel, "Remiel", Side::kPlayer, 250));
		for (const auto id : { kScrap1, kScrap2, kScrap3 }) {
			b.Join(Info(id, "Scrap", Side::kPlayer, 120, kRemiel));
			b.Find(id)->info.summon = true;
		}
		b.Join(Info(kWolf1, "Wolf", Side::kEnemy, 60));
		b.Join(Info(kWolf2, "Wolf", Side::kEnemy, 60));
		for (const auto id : { kWolf1, kWolf2 }) b.Find(id)->info.wouldFight = true;  // wolves attack on sight
		b.Damage(kWolf1, kScrap1, 60, 0.0f, 1);
		b.Death(kWolf1, kScrap1, 1);
		b.Damage(kScrap1, kWolf2, 120, 0.0f, 2);
		b.Death(kScrap1, kWolf2, 2);
		b.Death(kScrap2, 0, 5);  // dismissed: no blow, no killer
		b.Damage(kWolf2, kScrap3, 60, 0.0f, 6);
		b.Death(kWolf2, kScrap3, 6);
		b.endedAt = 10;
		// a corpse raised after it died is no summon: its first death stays a kill
		b.Join(Info(kWolf1, "Wolf", Side::kEnemy, 60, 0xFF000999));
		b.Find(kWolf1)->info.summon = false;
		ActorInfo raised = Info(kWolf1, "Wolf", Side::kEnemy, 60, 0xFF000999);
		raised.summon = true;
		b.Join(raised);
		Check(!b.Find(kWolf1)->info.summon, "a summon is decided when it first joins", "");
		const auto remiel = SummaryText(BuildSummary(b, kRemiel, "Remiel", {}));
		Check(!Has(remiel, "Scrap was killed") && !Has(remiel, "Scrap:") && Has(remiel, "Kaira and Remiel fought against Wolf x2."),
			"a summon is not a fighter of its own, and its end is no death", remiel);
		Check(Has(remiel, "Who killed the enemies: Remiel 2 (Wolf x2, 2 through Scrap)."), "its kills are its master's, with its name", remiel);
		Check(Has(remiel, "Remiel killed 2 of the 2 who died on the other side: Wolf x2.") && Has(remiel, "2 of them through Scrap."), "told to the master too", remiel);
		Check(!Has(BuildMemory(b, {}), "Scrap was"), "and nothing is remembered of its ends", BuildMemory(b, {}));
	}

	// The enemy's summons are no enemies of their own, but putting an end to one is still the fighter's doing.
	void TestEnemySummonsDispatched()
	{
		constexpr ActorId kNecro = 0xFF000921, kAtro1 = 0xFF000922, kAtro2 = 0xFF000923, kAtro3 = 0xFF000924;
		Battle b;
		b.omitBystanders = true;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
		b.Join(Info(kNecro, "Necromancer", Side::kEnemy, 150));
		for (const auto id : { kAtro1, kAtro2, kAtro3 }) {
			b.Join(Info(id, "Flame Atronach", Side::kEnemy, 100, kNecro));
			b.Find(id)->info.summon = true;
			b.Find(id)->info.wouldFight = true;
		}
		b.Damage(kLydia, kAtro1, 40, 0.86f, 1);
		b.Damage(kAtro1, kLydia, 100, 0.0f, 2);
		b.Death(kAtro1, kLydia, 2);
		b.Damage(kAtro2, kHero, 100, 0.0f, 3);
		b.Death(kAtro2, kHero, 3);
		b.Damage(kAtro3, kHero, 30, 0.7f, 3.8);  // struck once, then it vanished with its summoner: still nobody's doing
		b.Death(kAtro3, 0, 4);
		b.Damage(kNecro, kHero, 150, 0.0f, 4);
		b.Death(kNecro, kHero, 4);
		b.endedAt = 8;
		const auto told = SummaryText(BuildSummary(b, kLydia, "Lydia", {}));
		Check(Has(told, "The one enemy was killed.") && Has(told, "Who killed the enemies: Kaira 1 (Necromancer)."), "the enemy proper, apart from its summons", told);
		Check(Has(told, "Summoned creatures dispatched: Lydia 1 (Flame Atronach); Kaira 1 (Flame Atronach)."), "who dispatched them", told);
		const auto kaira = SummaryText(BuildSummary(b, kHero, "Kaira", {}));
		Check(Has(told, "Lydia also dispatched Flame Atronach.") && Has(kaira, "Kaira killed 1 of the 1 who died on the other side: Necromancer.") &&
				  Has(kaira, "Kaira also dispatched Flame Atronach."),
			"told to each who did", kaira);
	}

	// Seen in play: two Trolls, one that struck a follower down and one that had wounded her, read as one Troll killed twice.
	void TestSameNames()
	{
		constexpr ActorId kTroll1 = 0xFF000931, kTroll2 = 0xFF000932;
		Battle b;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kLydia, "Lydia", Side::kPlayer, 300));
		b.Join(Info(kTroll1, "Troll", Side::kEnemy, 300));
		b.Join(Info(kTroll2, "Troll", Side::kEnemy, 300));
		b.Damage(kLydia, kTroll2, 150, 0.5f, 1);
		b.Damage(kLydia, kTroll1, 140, 0.03f, 2);
		b.Down(kLydia, 2);
		b.Damage(kTroll1, kHero, 300, 0.0f, 4);
		b.Death(kTroll1, kHero, 4);
		b.Up(kLydia);
		b.Damage(kTroll2, kLydia, 300, 0.0f, 8);
		b.Death(kTroll2, kLydia, 8);
		b.endedAt = 10;
		const auto lydia = SummaryText(BuildSummary(b, kLydia, "Lydia", {}));
		Check(Has(lydia, "Lydia was struck down by the first Troll") && Has(lydia, "The first Troll, who had struck down Lydia, was then killed by Kaira.") &&
				  Has(lydia, "Lydia recovered enough to kill the second Troll, who had also wounded Lydia badly."),
			"same-named fighters are numbered, and only the one that did it struck her down", lydia);
		Check(Has(lydia, "Kaira and Lydia fought against Troll x2."), "lists keep the name alone", lydia);
		const auto hero = SummaryText(BuildSummary(b, kHero, "Kaira", {}));
		Check(Has(hero, "Lydia: ") && !Has(hero, "- the "), "every line starts with a capital", hero);
		Check(Has(hero, "Kaira saved Lydia's life. The first Troll, who had struck down Lydia, was then killed by Kaira."), "and every sentence within one", hero);
	}

	void TestDuration()
	{
		const auto is = [](double a_s, const char* a_want) { Check(DurationText(a_s) == a_want, a_want, DurationText(a_s)); };
		is(0.2, "1 second");
		is(42.4, "42 seconds");
		is(59.4, "59 seconds");
		is(59.8, "about 1 minute");
		is(60, "about 1 minute");
		is(170, "about 3 minutes");
		is(285, "about 5 minutes");
		is(300, "about 5 minutes");
		is(440, "about 5 minutes");
		is(460, "about 10 minutes");
		is(1560, "about 25 minutes");
		is(3600, "about 60 minutes");
		is(3700, "about 1 hour");
		is(5200, "about 1.5 hours");
		is(7300, "about 2 hours");
		const auto lydia = SummaryText(BuildSummary(Rescue(), kLydia, "Lydia", {}));
		Check(Has(lydia, "A battle was fought at Bleak Falls Barrow. It lasted 28 seconds."), "the battle's length is told", lydia);
	}

	void TestEdges()
	{
		Battle empty;
		Check(!BuildSummary(empty, kHero, "Kaira", {}).show, "no participants, nothing to show", "");
		Check(SummaryText(BuildSummary(empty, kHero, "Kaira", {})) == "(no battle to recall)", "empty summary text", "");

		Battle b;
		b.startedAt = 0;
		b.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		b.Join(Info(kBandit1, "Bandit", Side::kEnemy, 100));
		b.Damage(kHero, 0, 500, 0.0f, 1);  // a fall: no attacker
		b.Damage(kHero, kHero, 5, 0.0f, 1);  // self damage counts as no attacker
		b.Damage(kBandit1, 0x999, 10, 0.9f, 1);  // an attacker who never joined
		b.Damage(kBandit1, kHero, 1, 0.89f, 1);
		b.Heal(0x999, kHero, 10, "", 1);          // a target who never joined
		b.Death(0x999, kHero, 1);
		const auto ongoing = SummaryText(BuildSummary(b, kHero, "Kaira", {}));
		Check(Has(ongoing, "A battle is being fought right now.") && Has(ongoing, "So far 0 of the 1 enemies are dead."), "an ongoing battle", ongoing);
		Check(b.Find(kHero)->damageFrom.at(0) == 201.0f && b.Find(kHero)->kills.empty(), "unattributed damage, capped at the health it took, and unknown victims", ongoing);

		// overkill: a finishing blow counts for the health that was left
		Battle o;
		o.Join(Info(kHero, "Kaira", Side::kPlayer, 200));
		o.Join(Info(kBandit1, "Bandit", Side::kEnemy, 35));
		o.Join(Info(kBandit2, "Bandit", Side::kEnemy, 100));
		o.Damage(kBandit1, kHero, 1691.6f, 0.0f, 1);
		o.Damage(kBandit2, kHero, 60, 0.4f, 2);
		o.Damage(kBandit2, kHero, 500, 0.0f, 3);
		Check(o.Find(kBandit1)->damageTaken == 35.0f && o.Find(kBandit2)->damageTaken == 100.0f && o.Find(kHero)->damageDealt == 135.0f, "overkill is not counted", "");
		Check(!Significant(b, 3, 30.0) || b.Find(kHero)->WasCritical(), "significance", ongoing);
	}

	// Raised by another mod (a revive): the player lies in bleedout while a companion's atronach kills the one who struck
	// them down and a healer tends them; then the reviving mod reports the rise, before it restores their health. Nobody
	// saved their life. Struck down again later and saved for real, that second rescue is told as one.
	constexpr ActorId kVaelyra = 0xFF000901, kBrenna = 0xFF000902, kWraith1 = 0xFF000903, kWraith2 = 0xFF000904, kMagdra = 0xFF000905;
	constexpr const char* kSurge = "in a gust of wind and a surge of cold blue light, unhurt";

	Battle Risen(double a_killAt, bool a_end = true)
	{
		Battle b;
		b.id = 7;
		b.startedAt = 90.0;
		b.location = "Broken Fang Cave";
		b.Join(Info(kHero, "Arthur", Side::kPlayer, 400));
		b.Join(Info(kVaelyra, "Vaelyra", Side::kPlayer, 300));
		b.Join(Info(kBrenna, "Brenna", Side::kPlayer, 200));
		{
			auto a = Info(kMagdra, "Magdra Ibrai", Side::kPlayer, 300, kVaelyra);
			a.summon = true;
			b.Join(a);
		}
		b.Join(Info(kWraith1, "Wraith", Side::kEnemy, 250));
		b.Join(Info(kWraith2, "Wraith", Side::kEnemy, 250));
		b.Damage(kWraith1, kVaelyra, 40, 0.84f, 95);
		b.Damage(kHero, kWraith1, 380, 0.05f, 100);
		b.Down(kHero, 100.2);
		b.Heal(kHero, kBrenna, 90, "Healing Hands", 110);  // while he lay there
		b.Damage(kWraith1, kMagdra, 210, 0.0f, a_killAt);
		b.Death(kWraith1, kMagdra, a_killAt);
		{
			const auto lying = SummaryText(BuildSummary(b, kVaelyra, "Vaelyra", {})) + SummaryText(BuildSummary(b, kHero, "Arthur", {}));
			Check(!Has(lying, "saved Arthur's life") && !Has(lying, "was saved by") && !Has(lying, "without anyone's rescue")
					&& (a_killAt > 130.0 || Has(lying, "was then killed by Magdra Ibrai")),
				"while he still lies there: the kill is told, nobody has saved him yet", lying);
		}
		b.Revived(kHero, kSurge, 130.0);
		b.Sample(kHero, 0.02f, 130.4);  // the tick before the health comes back: still the same fall
		b.Down(kHero, 130.5);
		b.Heal(kHero, kBrenna, 380, "Healing Hands", 130.6);  // the restore, had the tracker taken it for healing
		b.Revived(kHero, kSurge, 131.0);  // told twice
		b.Up(kHero);
		if (!a_end) return b;
		b.Damage(kWraith2, kBrenna, 250, 0.0f, 160);
		b.Death(kWraith2, kBrenna, 160);
		b.endedAt = 165.0;
		return b;
	}

	void TestRisen()
	{
		for (const double killAt : { 115.0, 140.0 }) {  // the assailant killed while he lay there, and after he rose
			const auto b = Risen(killAt);
			const auto* hero = b.Find(kHero);
			Check(hero->downs == 1 && hero->Risen() && hero->risenAt == 130.0 && hero->criticalAt == 100.0, "one fall, one rise; the grace kept it one",
				std::format("downs {} risen {} at {} critical {}", hero->downs, hero->Risen(), hero->risenAt, hero->criticalAt));

			const auto arthur = SummaryText(BuildSummary(b, kHero, "Arthur", {}));
			Check(Has(arthur, "Arthur was struck down by the first Wraith"), "the fall is told", arthur);
			Check(Has(arthur, "Arthur got up again in a gust of wind and a surge of cold blue light, unhurt."), "the rise, in the reviver's words", arthur);
			Check(!Has(arthur, "saved Arthur's life") && !Has(arthur, "would most likely have died"), "nobody saved him", arthur);
			Check(!Has(arthur, "without anyone's rescue"), "no rescue verdict either way", arthur);
			Check(Has(arthur, "Vaelyra 1 (") && Has(arthur, "through Magdra Ibrai"), "the atronach's kill still counts as a kill", arthur);

			const auto vaelyra = SummaryText(BuildSummary(b, kVaelyra, "Vaelyra", {}));
			Check(!Has(vaelyra, "Vaelyra saved Arthur's life"), "the kill is no rescue", vaelyra);
			Check(Has(vaelyra, "Arthur: ") && Has(vaelyra, "got up again in a gust of wind"), "the others' line tells the rise", vaelyra);
			Check(!Has(vaelyra, "was saved by"), "and names no savior", vaelyra);

			const auto brenna = SummaryText(BuildSummary(b, kBrenna, "Brenna", {}));
			Check(!Has(brenna, "Brenna saved Arthur's life"), "healing while he lay there is no rescue", brenna);

			const auto memory = BuildMemory(b, {});
			Check(Has(memory, "Arthur was struck down by the first Wraith") && Has(memory, "and got up again in a gust of wind and a surge of cold blue light, unhurt."),
				"the remembered paragraph tells the rise", memory);
			Check(!Has(memory, "saved Arthur's life"), "and no rescue", memory);
		}

		// down again after the rise, and this time Vaelyra's atronach kills the one who did it: a real rescue
		{
			auto b = Risen(115.0, false);
			b.Damage(kHero, kWraith2, 390, 0.02f, 150);
			b.Down(kHero, 150.2);
			b.Damage(kWraith2, kMagdra, 250, 0.0f, 155);
			b.Death(kWraith2, kMagdra, 155);
			b.Up(kHero);
			b.endedAt = 170.0;
			const auto arthur = SummaryText(BuildSummary(b, kHero, "Arthur", {}));
			Check(b.Find(kHero)->downs == 2 && !b.Find(kHero)->Risen(), "a second fall, not risen from", arthur);
			Check(Has(arthur, "struck down twice"), "both falls", arthur);
			Check(Has(arthur, "got up again in a gust of wind"), "the first rise is still told", arthur);
			Check(Has(arthur, "Vaelyra saved Arthur's life"), "the second fall's rescue counts", arthur);
			Check(!Has(arthur, "Brenna saved"), "healing from before the rise saves nobody later", arthur);
		}

		// a rise reported for a fall the battle never saw: counted as a fall
		{
			Battle b;
			b.startedAt = 0.0;
			b.Join(Info(kHero, "Arthur", Side::kPlayer, 400));
			b.Join(Info(kWraith1, "Wraith", Side::kEnemy, 250));
			b.Damage(kHero, kWraith1, 100, 0.75f, 2);
			b.Revived(kHero, "", 5.0);
			b.endedAt = 10.0;
			const auto arthur = SummaryText(BuildSummary(b, kHero, "Arthur", {}));
			Check(b.Find(kHero)->downs == 1 && Has(arthur, "Arthur got up again."), "a missed fall counts; no words: a plain rise", arthur);
		}
	}
}

int main(int argc, char** argv)
{
	// build.ps1: write the reference file that ships with the mod
	if (argc == 3 && std::string(argv[1]) == "--write-reference") {
		std::FILE* f = std::fopen(argv[2], "wb");
		if (!f) return 2;
		const auto text = Text::Reference();
		std::fwrite(text.data(), 1, text.size(), f);
		std::fclose(f);
		return 0;
	}
	TestRescue();
	TestNearDeathAndSummon();
	TestOthers();
	TestBigFightAndBystanders();
	TestWildlifeAroundAFight();
	TestKillCreditFollowsTheFinalBlow();
	TestInjuriesAndDismemberment();
	TestInnocentsAndAllies();
	TestFoesSaveNobody();
	TestFriendlyFire();
	TestFriendlyFireOnBystanders();
	TestHarmlessEffects();
	TestKilledAtOnce();
	TestOwnWording();
	TestSayLessWhenInDoubt();
	TestNonCombatantsLeftOut();
	TestSummons();
	TestEnemySummonsDispatched();
	TestSameNames();
	TestDuration();
	TestEdges();
	TestRisen();
	std::printf("%d checks, %d failed\n", g_checks, g_failed);
	if (std::getenv("BSM_SHOW")) {
		const auto b = Rescue();
		std::printf("\n%s\n%s\n%s\n", SummaryText(BuildSummary(b, kLydia, "Lydia", {})).c_str(), SummaryText(BuildSummary(b, kHero, "Kaira", {})).c_str(),
			BuildMemory(b, {}).c_str());
	}
	return g_failed == 0 ? 0 : 1;
}
