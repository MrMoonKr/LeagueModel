#pragma once

#include "assets/asset_types.hpp"
#include "assets/asset_load_state.hpp"

#include <string>
#include <vector>

namespace LeagueModel::Assets
{
	struct LoadRecord
	{
		AssetEntry entry;
		AssetLoadState state = AssetLoadState::NotLoaded;
		std::string parentId;
		std::string error;
	};
}
