#pragma once

namespace LeagueModel::Assets
{
	enum class AssetLoadState
	{
		NotLoaded,
		Loading,
		Loaded,
		Partial,
		Failed
	};
}
