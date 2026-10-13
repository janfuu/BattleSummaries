#include "Narrative.h"

#include "Text.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace BSM
{
	namespace
	{
		// Every word told comes from Text (the player can reword it): T() for a phrase as it stands, F() to fill in its
		// {placeholders}. What a placeholder may leave empty carries its own leading space: Sp().
		std::string T(std::string_view a_key) { return Text::Get(a_key); }
		std::string F(std::string_view a_key, std::initializer_list<Text::Arg> a_args) { return Text::Fill(a_key, a_args); }
		std::string Sp(const std::string& a_text) { return a_text.empty() ? std::string() : " " + a_text; }
		std::string N(std::size_t a_n) { return std::to_string(a_n); }
		std::string N(int a_n) { return std::to_string(a_n); }

		// "a", "a and b", "a, b and c"
		std::string JoinList(const std::vector<std::string>& a_items)
		{
			std::string out;
			const auto  last = " " + T("list.and") + " ";
			for (std::size_t i = 0; i < a_items.size(); ++i) {
				if (i > 0) out += (i + 1 == a_items.size()) ? last : std::string(", ");
				out += a_items[i];
			}
			return out;
		}

		std::string NameOf(const Battle& a_b, ActorId a_id)
		{
			if (a_id == 0) return T("name.nobody");
			const auto* p = a_b.Find(a_id);
			return p && !p->info.name.empty() ? p->info.name : T("name.someone");
		}

		std::string CreditName(const Battle& a_b, ActorId a_id);

		// "was killed by Bandit Chief"; "was killed, by whom is not known" when the killer is nobody the battle saw
		std::string KilledBy(const Battle& a_b, ActorId a_killer)
		{
			if (a_killer == 0 || !a_b.Find(a_killer)) return T("killed.unknown");
			return F("killed.by", { { "killer", CreditName(a_b, a_killer) } });
		}

		// for kill and deed credit: a summon is named with its master
		std::string CreditName(const Battle& a_b, ActorId a_id)
		{
			const auto* p = a_b.Find(a_id);
			if (p && p->info.master != 0 && a_b.Find(p->info.master)) {
				return F("name.commanded", { { "name", NameOf(a_b, a_id) }, { "master", NameOf(a_b, p->info.master) } });
			}
			return NameOf(a_b, a_id);
		}

		// "Bandit x3, Bandit Chief": same-named actors are counted, in first-seen order; past a_max names, "and N others"
		std::string Grouped(const std::vector<const Participant*>& a_ps, std::size_t a_max = 4)
		{
			std::vector<std::pair<std::string, int>> groups;
			for (const auto* p : a_ps) {
				const auto n = p->info.name.empty() ? T("name.someone") : p->info.name;
				const auto  it = std::ranges::find_if(groups, [&](const auto& g) { return g.first == n; });
				if (it == groups.end()) groups.emplace_back(n, 1);
				else ++it->second;
			}
			std::vector<std::string> items;
			int                      rest = 0;
			for (const auto& [n, c] : groups) {
				// one name over the limit is shorter told than "and 1 other"
				if (items.size() < a_max || groups.size() == a_max + 1) items.push_back(c > 1 ? F("list.times", { { "name", n }, { "count", N(c) } }) : n);
				else rest += c;
			}
			if (rest > 0) items.push_back(F("list.others", { { "count", N(rest) } }));
			return JoinList(items);
		}

		std::vector<const Participant*> OnSide(const Battle& a_b, Side a_side)
		{
			std::vector<const Participant*> out;
			for (const auto& p : a_b.participants) {
				if (a_b.Involved(p) && a_b.EffectiveSide(p) == a_side) out.push_back(&p);
			}
			std::ranges::stable_partition(out, [](const Participant* p) { return p->info.isPlayer; });  // the player leads the list
			return out;
		}

		// someone else, on the same side: what they did to a_p was friendly fire
		bool OwnSide(const Battle& a_b, const Participant& a_p, ActorId a_other)
		{
			const auto* q = a_b.Find(a_other);
			return q && q->info.id != a_p.info.id && a_b.EffectiveSide(*q) == a_b.EffectiveSide(a_p);
		}

		// the damage a participant did to the other side: what they did to their own does not make them a fighter
		float DealtToFoes(const Battle& a_b, const Participant& a_p)
		{
			const Side side = a_b.EffectiveSide(a_p);
			float      sum = 0.0f;
			for (const auto& q : a_b.participants) {
				if (q.info.id == a_p.info.id || a_b.EffectiveSide(q) == side) continue;
				if (const auto it = q.damageFrom.find(a_p.info.id); it != q.damageFrom.end()) sum += it->second;
			}
			return sum;
		}

		float SideDealtToFoes(const Battle& a_b, Side a_side)
		{
			float sum = 0.0f;
			for (const auto& p : a_b.participants) {
				if (a_b.EffectiveSide(p) == a_side) sum += DealtToFoes(a_b, p);
			}
			return sum;
		}

		Side Opposing(Side a_side) { return a_side == Side::kEnemy ? Side::kPlayer : Side::kEnemy; }

		std::string Points(float a_amount, const NarrativeOptions& a_opt)
		{
			return a_opt.showNumbers ? " " + F("points", { { "points", N(static_cast<int>(std::lround(a_amount))) } }) : std::string();
		}


		// the damage their own side did to them, and who did a part of it worth naming
		float FriendlyDamage(const Battle& a_b, const Participant& a_p, std::vector<ActorId>* a_by = nullptr)
		{
			float sum = 0.0f;
			for (const auto& [id, dmg] : a_p.damageFrom) {
				if (id == 0 || !OwnSide(a_b, a_p, id)) continue;
				sum += dmg;
				if (a_by && dmg >= 0.05f * a_p.info.maxHealth) a_by->push_back(id);
			}
			return sum;
		}

		std::string Wounds(const Battle& a_b, const Participant& a_p, const NarrativeOptions& a_opt)
		{
			// with friendly fire left out, what their own side did to them is no part of the wounds told
			const float taken = a_opt.ignoreFriendlyFire ? std::max(0.0f, a_p.damageTaken - FriendlyDamage(a_b, a_p)) : a_p.damageTaken;
			const float r = a_p.info.maxHealth > 0.0f ? taken / a_p.info.maxHealth : 0.0f;
			std::string w;
			if (r < 0.03f) w = T("wounds.none");
			else if (r < 0.25f) w = T("wounds.light");
			else if (r < 0.6f) w = T("wounds.solid");
			else if (r < 1.0f) w = T("wounds.bad");
			else if (a_p.healingReceived > 0.0f) w = T("wounds.beyond_healed");
			else w = T("wounds.brutal");
			return w + Points(taken, a_opt);
		}

		std::string Dealt(const Battle& a_b, const Participant& a_p, const NarrativeOptions& a_opt)
		{
			const Side  side = a_b.EffectiveSide(a_p);
			const float total = SideDealtToFoes(a_b, side);
			const float dealt = DealtToFoes(a_b, a_p);
			if (dealt <= 0.0f) return T("dealt.none");
			std::string w;
			if (a_b.Count(side) <= 1 || total <= 0.0f) w = T("dealt.all");
			else {
				const float s = dealt / total;
				if (s < 0.15f) w = T("dealt.small");
				else if (s < 0.4f) w = T("dealt.fair");
				else if (s < 0.7f) w = T("dealt.large");
				else w = T("dealt.most");
			}
			return w + Points(dealt, a_opt);
		}

		std::string HealAmount(const Participant& a_p, float a_amount, const NarrativeOptions& a_opt)
		{
			const float r = a_p.info.maxHealth > 0.0f ? a_amount / a_p.info.maxHealth : 0.0f;
			const auto w = T(r < 0.3f ? "heal.little" : r < 0.7f ? "heal.good" : "heal.most");
			return F("heal.restoring", { { "amount", w }, { "name", a_p.info.name }, { "points", Points(a_amount, a_opt) } });
		}

		// Who pulled a participant back from the brink, and how.
		struct Rescue
		{
			std::vector<ActorId>     saviors;
			std::vector<std::string> deeds;  // full sentences
			std::vector<std::string> brief;  // "A killed X", "A healed Y", for the summary
		};

		Rescue FindRescue(const Battle& a_b, const Participant& a_p, const NarrativeOptions& a_opt)
		{
			Rescue r;
			if (!a_p.WasCritical() || a_p.dead) return r;
			const auto addSavior = [&](ActorId a_id) {
				if (a_id != 0 && a_id != a_p.info.id && std::ranges::find(r.saviors, a_id) == r.saviors.end()) r.saviors.push_back(a_id);
			};
			const Side  mine = a_b.EffectiveSide(a_p);
			const auto  what = T(a_p.downs > 0 ? "rescue.struck_down" : "rescue.nearly_killed");

			// the assailants: whoever dealt the blows that did it, and anyone who dealt a large part of all the damage
			std::vector<ActorId> assailants;
			if (a_p.broughtLowBy != 0) assailants.push_back(a_p.broughtLowBy);
			for (const auto& [id, dmg] : a_p.damageFrom) {
				if (id != 0 && dmg >= 0.4f * a_p.damageTaken && std::ranges::find(assailants, id) == assailants.end()) assailants.push_back(id);
			}
			for (const auto id : assailants) {
				const auto* a = a_b.Find(id);
				if (!a || !a->dead || a->diedAt < a_p.criticalAt || a_b.EffectiveSide(*a) == mine) continue;
				if (a->killer == a_p.info.id) {
					r.deeds.push_back(F("rescue.recovered", { { "name", a_p.info.name }, { "assailant", a->info.name }, { "what", what } }));
				} else if (a->killer != 0) {
					r.deeds.push_back(F("rescue.assailant_killed",
						{ { "assailant", a->info.name }, { "what", what }, { "name", a_p.info.name }, { "killer", CreditName(a_b, a->killer) } }));
					// a foe who happened to kill the assailant (monsters turning on each other) saved nobody
					const auto* k = a_b.Find(a->killer);
					if (!k || a_b.EffectiveSide(*k) == Opposing(mine)) continue;
					r.brief.push_back(F("rescue.brief_killed", { { "killer", NameOf(a_b, a->killer) }, { "assailant", a->info.name } }));
					addSavior(k->info.master != 0 && a_b.Find(k->info.master) ? k->info.master : a->killer);
				}
			}
			for (const auto& [healer, h] : a_p.healFrom) {
				if (healer == 0 || healer == a_p.info.id || h.whileCritical < 0.15f * a_p.info.maxHealth) continue;
				r.deeds.push_back(F("rescue.healed_while",
					{ { "name", a_p.info.name },
						{ "state", T(a_p.downs > 0 ? "rescue.state_down" : "rescue.state_deaths_door") },
						{ "healer", NameOf(a_b, healer) },
						{ "with", h.source.empty() ? std::string() : " " + F("heal.with", { { "source", h.source } }) },
						{ "restoring", HealAmount(a_p, h.whileCritical, a_opt) } }));
				r.brief.push_back(F("rescue.brief_healed", { { "healer", NameOf(a_b, healer) }, { "name", a_p.info.name } }));
				addSavior(healer);
			}
			return r;
		}

		std::vector<std::string> Names(const Battle& a_b, const std::vector<ActorId>& a_ids)
		{
			std::vector<std::string> out;
			for (const auto id : a_ids) out.push_back(NameOf(a_b, id));
			return out;
		}

		// "was struck down (collapsed, bleeding out) twice by X" / "was brought to the edge of death by X" / ""
		std::string Brink(const Battle& a_b, const Participant& a_p, const NarrativeOptions& a_opt)
		{
			std::string by = a_p.broughtLowBy != 0 ? " " + F("brink.by", { { "name", NameOf(a_b, a_p.broughtLowBy) } }) : std::string();
			if (OwnSide(a_b, a_p, a_p.broughtLowBy)) by = a_opt.ignoreFriendlyFire ? std::string() : by + " " + T("brink.friendly");
			if (a_p.downs > 0) {
				const std::string times = a_p.downs == 1 ? std::string() : " " + (a_p.downs == 2 ? T("times.twice") : F("times.many", { { "count", N(a_p.downs) } }));
				return F("brink.down", { { "times", times }, { "by", by } });
			}
			if (a_p.nearDeath) return F("brink.near_death", { { "by", by } });
			return {};
		}

		// enemy-of-a_side kills per killer, most first: "Kaira 3 (Bandit x2, Bandit Chief), Lydia 1 (Bandit)"
		std::string KillTally(const Battle& a_b, Side a_victims)
		{
			std::vector<std::pair<ActorId, std::vector<const Participant*>>> tally;
			for (const auto* v : OnSide(a_b, a_victims)) {
				if (!v->dead) continue;
				const auto it = std::ranges::find_if(tally, [&](const auto& t) { return t.first == v->killer; });
				if (it == tally.end()) tally.push_back({ v->killer, { v } });
				else it->second.push_back(v);
			}
			std::ranges::stable_sort(tally, [](const auto& a, const auto& b) { return a.second.size() > b.second.size(); });
			std::vector<std::string> items;
			for (const auto& [killer, victims] : tally) {
				// the victims by name only while the list stays short
				items.push_back((killer == 0 ? T("tally.unknown") : CreditName(a_b, killer)) + " " + N(victims.size()) +
								(victims.size() <= 3 ? " (" + Grouped(victims) + ")" : std::string()));
			}
			std::string out;
			for (const auto& i : items) out += (out.empty() ? "" : "; ") + i;
			return out;
		}

		std::string Outcome(const Battle& a_b, Side a_foes)
		{
			const int n = a_b.Count(a_foes), dead = a_b.Dead(a_foes);
			if (n == 0) return {};
			const std::initializer_list<Text::Arg> args{ { "dead", N(dead) }, { "total", N(n) }, { "foes", T(a_foes == Side::kEnemy ? "outcome.word_enemies" : "outcome.word_opponents") } };
			if (a_b.Ongoing()) return F("outcome.so_far", args);
			if (dead == n) return n == 1 ? T("outcome.one") : F("outcome.all", args);
			if (dead == 0) return F("outcome.none", args);
			return F("outcome.some", args);
		}

		std::string EffectPhrase(const Battle& a_b, const Participant& a_p, const EffectNote& a_e)
		{
			std::string s = a_e.label;
			if (a_e.by != 0 && a_e.by != a_p.info.id) {
				s += " " + F("effect.by", { { "name", NameOf(a_b, a_e.by) } });
				if (!a_e.source.empty()) s += " (" + a_e.source + ")";
			} else if (!a_e.source.empty()) {
				s += " (" + (a_e.by == 0 ? a_e.source : F("effect.own", { { "source", a_e.source } })) + ")";  // by nobody: a trap, a hazard
			}
			return s;
		}

		std::string Effects(const Battle& a_b, const Participant& a_p, bool a_hostile, int a_max)
		{
			std::vector<std::string> items;
			int                      more = 0;
			for (const auto& e : a_p.effects) {
				if (e.hostile != a_hostile) continue;
				if (e.hostile && OwnSide(a_b, a_p, e.by)) continue;  // friendly fire: told apart (FriendlyFire), or not at all
				// the same thing from several attackers of one name (three wolves) is told once
				auto phrase = EffectPhrase(a_b, a_p, e);
				if (std::ranges::find(items, phrase) != items.end()) continue;
				if (static_cast<int>(items.size()) < a_max) items.push_back(std::move(phrase));
				else ++more;
			}
			if (items.empty()) return {};
			auto s = JoinList(items);
			if (more > 0) s += ", " + F("effects.more", { { "count", N(more) } });
			return s;
		}

		// "Leg Injury (dealt by Bandit Chief) and Head Injury"; a_carried picks those brought into the battle
		std::string Injuries(const Battle& a_b, const Participant& a_p, bool a_carried, bool a_withDealer = true)
		{
			std::vector<std::string> items;
			for (const auto& i : a_p.injuries) {
				if (i.carried != a_carried) continue;
				items.push_back(a_withDealer && i.by != 0 ? F("injury.dealt_by", { { "injury", i.name }, { "name", NameOf(a_b, i.by) } }) : i.name);
			}
			return JoinList(items);
		}

		// Bodies that lost a head or a limb, per killer: "Lydia beheaded Bandit Chief."
		std::vector<std::string> Maimings(const Battle& a_b)
		{
			struct Group
			{
				ActorId                         killer;
				bool                            beheaded;
				std::vector<const Participant*> victims;
			};
			std::vector<Group> groups;
			for (const auto& p : a_b.participants) {
				if (!p.dead || !p.dismembered || !a_b.Involved(p)) continue;
				const auto it = std::ranges::find_if(groups, [&](const Group& g) { return g.killer == p.killer && g.beheaded == p.beheaded; });
				if (it == groups.end()) groups.push_back({ p.killer, p.beheaded, { &p } });
				else it->victims.push_back(&p);
			}
			std::vector<std::string> out;
			for (const auto& g : groups) {
				if (out.size() >= 4) break;  // a slaughter needs no full list
				const auto who = Grouped(g.victims);
				if (g.killer == 0) out.push_back(F(g.beheaded ? "maim.head_unknown" : "maim.limb_unknown", { { "victims", who } }));
				else out.push_back(F(g.beheaded ? "maim.beheaded" : "maim.dismembered", { { "killer", CreditName(a_b, g.killer) }, { "victims", who } }));
			}
			return out;
		}

		// what their own side's attacks did to them: "burned by fire by Kaira (Fireball)"
		std::string FriendlyFire(const Battle& a_b, const Participant& a_p, int a_max)
		{
			std::vector<std::string> items;
			for (const auto& e : a_p.effects) {
				if (!e.hostile || !OwnSide(a_b, a_p, e.by) || static_cast<int>(items.size()) >= a_max) continue;
				auto phrase = EffectPhrase(a_b, a_p, e);
				if (std::ranges::find(items, phrase) == items.end()) items.push_back(std::move(phrase));
			}
			return JoinList(items);
		}

		// Several foes killed by one fighter in the same instant (a fireball, a shout, a cleaving blow): "Kaira killed 3
		// at once: Bandit x2 and Bandit Chief." The largest first, three lines at most.
		std::vector<std::string> KilledAtOnce(const Battle& a_b)
		{
			constexpr double kSameInstant = 0.75;  // seconds between deaths that still count as one stroke
			struct Stroke
			{
				ActorId                         killer;
				std::vector<const Participant*> victims;
			};
			std::vector<Stroke> strokes;
			for (const auto& k : a_b.participants) {
				if (k.kills.size() < 2) continue;
				const Side                      foes = Opposing(a_b.EffectiveSide(k));
				std::vector<const Participant*> dead;
				for (const auto id : k.kills) {
					const auto* v = a_b.Find(id);
					if (v && v->dead && v->diedAt >= 0.0 && a_b.Involved(*v) && a_b.EffectiveSide(*v) == foes) dead.push_back(v);
				}
				std::ranges::stable_sort(dead, [](const Participant* a, const Participant* b) { return a->diedAt < b->diedAt; });
				for (std::size_t i = 0; i < dead.size();) {
					std::size_t j = i + 1;
					while (j < dead.size() && dead[j]->diedAt - dead[j - 1]->diedAt <= kSameInstant) ++j;
					if (j - i >= 2) strokes.push_back({ k.info.id, { dead.begin() + i, dead.begin() + j } });
					i = j;
				}
			}
			std::ranges::stable_sort(strokes, [](const Stroke& a, const Stroke& b) { return a.victims.size() > b.victims.size(); });
			std::vector<std::string> out;
			for (const auto& st : strokes) {
				if (out.size() >= 3) break;
				// how, when one spell or weapon did all of it
				std::string how = st.victims.front()->killedWith;
				for (const auto* v : st.victims) {
					if (v->killedWith != how) how.clear();
				}
				out.push_back(F("moment.at_once",
					{ { "killer", CreditName(a_b, st.killer) },
						{ "count", N(st.victims.size()) },
						{ "with", how.empty() ? std::string() : " " + F("moment.with", { { "means", how } }) },
						{ "victims", Grouped(st.victims) } }));
			}
			return out;
		}

		int EnemyKills(const Battle& a_b, const Participant& a_p, Side a_foes, std::vector<const Participant*>* a_out = nullptr)
		{
			int n = 0;
			for (const auto id : a_p.kills) {
				const auto* v = a_b.Find(id);
				if (v && a_b.EffectiveSide(*v) == a_foes) {
					++n;
					if (a_out) a_out->push_back(v);
				}
			}
			return n;
		}

		std::vector<std::string> Personal(const Battle& a_b, const Participant& a_p, const NarrativeOptions& a_opt)
		{
			std::vector<std::string> out;
			const auto&              n = a_p.info.name;
			const Side               foes = Opposing(a_b.EffectiveSide(a_p));

			out.push_back(F("personal.wounds_dealt", { { "name", n }, { "wounds", Wounds(a_b, a_p, a_opt) }, { "dealt", Dealt(a_b, a_p, a_opt) } }));

			if (a_p.dead) {
				out.push_back(F("killed.line", { { "name", n }, { "killed", KilledBy(a_b, a_p.killer) }, { "own_side", "" } }));
			} else if (const auto brink = Brink(a_b, a_p, a_opt); !brink.empty()) {
				out.push_back(F("personal.fact", { { "name", n }, { "fact", brink } }));
			} else if (a_p.startHealthPct < a_b.nearDeathPct) {
				out.push_back(F("personal.already_low", { { "name", n } }));
			} else if (a_p.minHealthPct < 0.4f) {
				out.push_back(F("personal.hurt_badly", { { "name", n } }));
			} else {
				out.push_back(F("personal.no_danger", { { "name", n } }));
			}

			const auto rescue = FindRescue(a_b, a_p, a_opt);
			for (const auto& d : rescue.deeds) out.push_back(d);

			// healing not already told as part of a rescue
			for (const auto& [healer, h] : a_p.healFrom) {
				const bool told = healer != a_p.info.id && h.whileCritical >= 0.15f * a_p.info.maxHealth && a_p.WasCritical() && !a_p.dead;
				if (told || h.amount < 0.1f * a_p.info.maxHealth) continue;
				const std::string with = h.source.empty() ? std::string() : " " + F("heal.with", { { "source", h.source } });
				const auto        restoring = HealAmount(a_p, h.amount, a_opt);
				if (healer == a_p.info.id || healer == 0) out.push_back(F("heal.self", { { "name", n }, { "with", with }, { "restoring", restoring } }));
				else out.push_back(F("heal.other", { { "healer", NameOf(a_b, healer) }, { "name", n }, { "with", with }, { "restoring", restoring } }));
			}

			if (!rescue.saviors.empty()) {
				out.push_back(F("rescue.plain", { { "saviors", JoinList(Names(a_b, rescue.saviors)) }, { "name", n } }));
			} else if (a_p.WasCritical() && !a_p.dead && rescue.deeds.empty()) {
				out.push_back(F("rescue.none", { { "name", n } }));
			}

			if (const auto old = Injuries(a_b, a_p, true); !old.empty()) out.push_back(F("injury.carried", { { "name", n }, { "injuries", old } }));
			if (const auto hurt = Injuries(a_b, a_p, false); !hurt.empty() && !a_p.dead) {
				out.push_back(F("injury.new", { { "name", n }, { "injuries", hurt } }));
			}
			if (const auto bad = Effects(a_b, a_p, true, a_opt.maxEffects); !bad.empty()) out.push_back(F("effects.suffered", { { "name", n }, { "effects", bad } }));
			if (!a_opt.ignoreFriendlyFire) {
				// one line for all of it: the blows, then the magic
				std::vector<ActorId> by;
				std::string          own;
				if (FriendlyDamage(a_b, a_p, &by) >= 0.1f * a_p.info.maxHealth && !by.empty()) own = F("friendly.damage", { { "names", JoinList(Names(a_b, by)) } });
				if (const auto magic = FriendlyFire(a_b, a_p, a_opt.maxEffects); !magic.empty()) own += (own.empty() ? "" : "; ") + magic;
				if (!own.empty()) out.push_back(F("friendly.line", { { "name", n }, { "what", own } }));
			}
			if (const auto good = Effects(a_b, a_p, false, a_opt.maxEffects); !good.empty()) out.push_back(F("effects.helped", { { "name", n }, { "effects", good } }));

			std::vector<const Participant*> victims;
			const int                       mine = EnemyKills(a_b, a_p, foes, &victims);
			const int                       all = a_b.Dead(foes);
			if (mine > 0) {
				out.push_back(F("kills.mine", { { "name", n }, { "count", N(mine) }, { "total", N(all) }, { "victims", mine <= 3 ? ": " + Grouped(victims) : std::string() } }));
			}
			else if (all > 0) out.push_back(F("kills.none", { { "name", n }, { "total", N(all) } }));

			// the lives this participant saved
			for (const auto& q : a_b.participants) {
				if (q.info.id == a_p.info.id) continue;
				const auto r = FindRescue(a_b, q, a_opt);
				if (std::ranges::find(r.saviors, a_p.info.id) != r.saviors.end()) {
					std::string deeds;
					for (const auto& d : r.deeds) deeds += " " + d;
					out.push_back(F("rescue.saved_other", { { "name", n }, { "other", q.info.name }, { "deeds", deeds } }));
				}
			}
			return out;
		}

		std::string OtherLine(const Battle& a_b, const Participant& a_p, const NarrativeOptions& a_opt)
		{
			const Side foes = Opposing(a_b.EffectiveSide(a_p));
			std::vector<std::string> bits{ Wounds(a_b, a_p, a_opt) };
			if (a_p.dead) bits.push_back(KilledBy(a_b, a_p.killer));
			else if (const auto brink = Brink(a_b, a_p, a_opt); !brink.empty()) {
				bits.push_back(brink);
				const auto r = FindRescue(a_b, a_p, a_opt);
				if (!r.saviors.empty()) bits.push_back(F("other.saved_by", { { "saviors", JoinList(Names(a_b, r.saviors)) } }));
			} else if (a_p.startHealthPct < a_b.nearDeathPct) {
				bits.push_back(T("other.already_low"));
			} else bits.push_back(T("other.no_danger"));
			if (const auto hurt = Injuries(a_b, a_p, false, false); !hurt.empty() && !a_p.dead) bits.push_back(F("other.injured", { { "injuries", hurt } }));
			const int kills = EnemyKills(a_b, a_p, foes);
			bits.push_back(kills == 0 ? T("other.kills_none") : F("other.kills", { { "count", N(kills) } }));
			std::string facts;
			for (std::size_t i = 0; i < bits.size(); ++i) facts += (i ? "; " : "") + bits[i];
			return F("other.line", { { "name", a_p.info.name }, { "facts", facts } });
		}

		// nobody on this side fell, went down, came near death or took real wounds
		bool CameThroughEasily(const Battle& a_b, Side a_side)
		{
			for (const auto* p : OnSide(a_b, a_side)) {
				if (p->dead || p->WasCritical() || p->damageTaken >= 0.25f * p->info.maxHealth || p->startHealthPct < a_b.nearDeathPct) return false;
				if (std::ranges::any_of(p->injuries, [](const InjuryNote& i) { return !i.carried; })) return false;
			}
			return true;
		}

		std::vector<std::string> General(const Battle& a_b)
		{
			std::vector<std::string> out;
			const auto               party = OnSide(a_b, Side::kPlayer), enemies = OnSide(a_b, Side::kEnemy), others = OnSide(a_b, Side::kOther);
			const std::string        where = a_b.location.empty() ? std::string() : " " + F("battle.at", { { "place", a_b.location } });
			if (a_b.Ongoing()) out.push_back(F("battle.underway", { { "at_place", where } }));
			else out.push_back(F("battle.fought", { { "at_place", where }, { "duration", DurationText(a_b.endedAt - a_b.startedAt) } }));

			if (!party.empty() && !enemies.empty()) out.push_back(F("battle.sides", { { "party", Grouped(party) }, { "enemies", Grouped(enemies) } }));
			else if (!enemies.empty()) out.push_back(F("battle.enemies_only", { { "enemies", Grouped(enemies) } }));
			if (!others.empty()) out.push_back(F("battle.caught_up", { { "others", Grouped(others) } }));

			if (auto o = Outcome(a_b, Side::kEnemy); !o.empty()) out.push_back(std::move(o));
			if (const auto t = KillTally(a_b, Side::kEnemy); !t.empty()) out.push_back(F("tally.line", { { "tally", t } }));
			for (const auto* p : party) {
				if (!p->dead) continue;
				// killed by their own side: said outright, accident or not
				const auto* k = a_b.Find(p->killer);
				const bool  ownSide = k && k->info.id != p->info.id && a_b.EffectiveSide(*k) == Side::kPlayer;
				out.push_back(F("killed.line", { { "name", p->info.name }, { "killed", KilledBy(a_b, p->killer) }, { "own_side", ownSide ? ", " + T("killed.own_side") : std::string() } }));
			}
			for (const auto* p : others) {
				if (!p->dead || !a_b.Innocent(*p)) continue;
				out.push_back(F("killed.not_fighting", { { "name", p->info.name }, { "killed", KilledBy(a_b, p->killer) } }));
			}
			for (auto& m : KilledAtOnce(a_b)) out.push_back(std::move(m));
			for (auto& m : Maimings(a_b)) out.push_back(std::move(m));
			return out;
		}
	}

	Summary BuildSummary(const Battle& a_b, ActorId a_viewer, const std::string& a_viewerName, const NarrativeOptions& a_opt)
	{
		Summary r;
		if (a_b.participants.empty()) return r;
		r.show = true;
		r.ongoing = a_b.Ongoing();
		r.name = a_viewerName;
		r.general = General(a_b);

		const auto* me = a_b.Find(a_viewer);
		if (me && !a_b.Involved(*me)) me = nullptr;  // in the game's combat state, but never fought: a witness
		r.participant = me != nullptr;
		if (me) {
			if (r.name.empty()) r.name = me->info.name;
			r.personal = Personal(a_b, *me, a_opt);
		}
		// their companions: the viewer's own side, or the player's side for a witness
		const Side side = me && a_b.EffectiveSide(*me) == Side::kEnemy ? Side::kEnemy : Side::kPlayer;
		// an easy fight needs no roll call
		if (CameThroughEasily(a_b, side)) {
			if (OnSide(a_b, side).size() > (me ? 1u : 0u)) r.others.push_back(T("other.easy"));
			return r;
		}
		for (const auto* p : OnSide(a_b, side)) {
			if (p->info.id == a_viewer) continue;
			if (static_cast<int>(r.others.size()) >= a_opt.maxOthers) break;
			r.others.push_back(OtherLine(a_b, *p, a_opt));
		}
		return r;
	}

	std::string BuildMemory(const Battle& a_b, const NarrativeOptions& a_opt)
	{
		std::string s;
		const auto  add = [&](const std::string& a_line) {
			if (a_line.empty()) return;
			if (!s.empty()) s += ' ';
			s += a_line;
		};
		for (const auto& l : General(a_b)) add(l);
		for (const auto* p : OnSide(a_b, Side::kPlayer)) {
			if (p->dead) continue;
			if (const auto hurt = Injuries(a_b, *p, false); !hurt.empty()) add(F("injury.memory", { { "name", p->info.name }, { "injuries", hurt } }));
			if (!p->WasCritical()) continue;
			const auto        r = FindRescue(a_b, *p, a_opt);
			const std::string brink = Brink(a_b, *p, a_opt);
			if (!r.saviors.empty()) {
				add(F("memory.rescued", { { "name", p->info.name }, { "brink", brink }, { "deeds", JoinList(r.brief) }, { "saviors", JoinList(Names(a_b, r.saviors)) } }));
			} else {
				add(F("memory.survived", { { "name", p->info.name }, { "brink", brink } }));
			}
		}
		return s;
	}

	std::string DurationText(double a_seconds)
	{
		if (a_seconds < 60.0) {
			const int s = std::max(1, static_cast<int>(std::lround(a_seconds)));
			if (s < 60) return s == 1 ? T("duration.second") : F("duration.seconds", { { "count", N(s) } });
		}
		const double minutes = a_seconds / 60.0;
		if (minutes < 5.0) {
			const int m = std::max(1, static_cast<int>(std::lround(minutes)));
			if (m < 5) return m == 1 ? T("duration.minute") : F("duration.minutes", { { "count", N(m) } });
		}
		if (minutes <= 60.0) return F("duration.minutes", { { "count", N(std::max(5, static_cast<int>(std::lround(minutes / 5.0)) * 5)) } });
		const int halves = std::max(2, static_cast<int>(std::lround(minutes / 30.0)));  // half hours
		if (halves == 2) return T("duration.hour");
		return F(halves % 2 == 0 ? "duration.hours" : "duration.hours_half", { { "count", N(halves / 2) } });
	}

	std::string SummaryText(const Summary& a_r)
	{
		if (!a_r.show) return "(no battle to recall)";
		std::string s = a_r.ongoing ? "BATTLE UNDERWAY\n" : "BATTLE JUST FOUGHT\n";
		for (const auto& l : a_r.general) s += "- " + l + "\n";
		if (!a_r.personal.empty()) {
			s += std::format("What happened to {}:\n", a_r.name);
			for (const auto& l : a_r.personal) s += "- " + l + "\n";
		}
		if (!a_r.others.empty()) {
			s += "The others:\n";
			for (const auto& l : a_r.others) s += "- " + l + "\n";
		}
		return s;
	}

	bool Significant(const Battle& a_b, int a_minEnemies, double a_minSeconds)
	{
		for (const auto& p : a_b.participants) {
			if (p.dead && a_b.Innocent(p)) return true;  // an innocent died
		}
		if (a_b.Count(Side::kPlayer) == 0 || a_b.Count(Side::kEnemy) == 0) return false;  // nothing the player's side fought
		for (const auto* p : OnSide(a_b, Side::kPlayer)) {
			if (p->dead || p->WasCritical()) return true;
			if (std::ranges::any_of(p->injuries, [](const InjuryNote& i) { return !i.carried; })) return true;
		}
		if (a_b.Count(Side::kEnemy) >= a_minEnemies) return true;
		return !a_b.Ongoing() && a_b.endedAt - a_b.startedAt >= a_minSeconds;
	}
}
