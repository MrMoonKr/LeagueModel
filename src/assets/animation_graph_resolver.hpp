#pragma once

#include "assets/bin_document.hpp"
#include "assets/game_hash_index.hpp"

#include <map>

namespace LeagueModel::Assets
{
	struct AnimationGraphAssets
	{
		std::map<std::uint32_t, std::string> clipResources;
		size_t clipCount = 0;
		size_t maskCount = 0;
		size_t trackCount = 0;
	};

	bool ResolveAnimationGraphAssets(const BinDocument& document, const std::string& characterName, std::uint8_t skinIndex,
		const GameHashIndex& hashIndex, AnimationGraphAssets& result, std::string* error = nullptr);
}
