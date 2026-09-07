#pragma once

#include "assets/bin_document.hpp"
#include "assets/game_hash_index.hpp"

#include <optional>
#include <unordered_map>

namespace LeagueModel::Assets
{
	struct SkinAssetReferences
	{
		std::string skinMeshPath;
		std::string skeletonPath;
		std::string texturePath;
		std::unordered_map<std::uint32_t, std::string> submeshTexturePaths;
		std::optional<std::uint32_t> animationGraphHash;
	};

	bool ResolveSkinAssets(const BinDocument& document, const std::string& characterName, std::uint8_t skinIndex,
		const GameHashIndex& hashIndex, SkinAssetReferences& result, std::string* error = nullptr);
}
