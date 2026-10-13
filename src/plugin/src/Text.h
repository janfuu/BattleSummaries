#pragma once

// Every piece of text the mod writes into a summary, by key, with its built-in wording. A player can reword any of
// them in BattleSummaries_Text.ini (see Reference()); a line that is missing, empty or uses a placeholder its phrase
// does not have falls back to the built-in wording, so nothing typed there can blank a summary.
//
// No game types: shared by the plugin and the offline tests.

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace BSM::Text
{
	struct Entry
	{
		std::string_view group;  // the heading it is listed under in the reference file
		std::string_view key;    // unique across all groups
		std::string_view text;   // the built-in wording; {placeholders} are filled in by the mod
		std::string_view note;   // what it is for; may be empty
	};

	[[nodiscard]] const std::vector<Entry>& Defaults();

	// The wording in force for a key: the player's, else the built-in. An unknown key gives an empty string.
	[[nodiscard]] std::string Get(std::string_view a_key);

	// Get(), with each {name} replaced by its value.
	using Arg = std::pair<std::string_view, std::string>;
	[[nodiscard]] std::string Fill(std::string_view a_key, std::initializer_list<Arg> a_args);

	// Whether a player's wording for a key can be used. a_why says why not: no such key, or a placeholder the phrase
	// does not have.
	[[nodiscard]] bool Accepts(std::string_view a_key, std::string_view a_text, std::string* a_why = nullptr);

	// Replaces all of the player's wordings at once (those that Accepts() refuses are left out). Returns how many
	// were taken.
	std::size_t SetOverrides(const std::vector<std::pair<std::string, std::string>>& a_lines);

	// The reference file: every phrase with its built-in wording, grouped and commented. Written by the build into
	// BattleSummaries_Text.default.ini, which ships with the mod and is replaced on every update.
	[[nodiscard]] std::string Reference();
}
