#pragma once

#include "assets/asset_system.hpp"
#include "assets/character_asset_resolver.hpp"
#include "assets/skeleton_document.hpp"
#include "assets/skin_document.hpp"
#include "assets/texture_document.hpp"

#include <unordered_map>

namespace LeagueModel::Assets
{
	struct StaticCharacterAssets
	{
		SkinAssetReferences references;
		SkinDocument skin;
		SkeletonDocument skeleton;
		TextureDocument texture;
		std::unordered_map<std::uint32_t, TextureDocument> submeshTextures;
	};

	bool LoadStaticCharacterAssets(AssetSystem& assets, const std::string& characterName, std::uint8_t skinIndex,
		const GameHashIndex& hashes, StaticCharacterAssets& result, std::string* error = nullptr);
}
