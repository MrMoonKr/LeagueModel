#pragma once

namespace LeagueModel
{
	struct Character;
	namespace Assets { class AssetSystem; }
	void RenderUI(Character& character, const Assets::AssetSystem* assetSystem = nullptr);
}
