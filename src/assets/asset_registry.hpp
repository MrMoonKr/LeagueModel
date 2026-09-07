#pragma once

#include "assets/asset_types.hpp"

#include <functional>
#include <optional>
#include <unordered_map>

namespace LeagueModel::Assets
{
	class AssetRegistry
	{
	public:
		using PayloadProvider = std::function<std::vector<std::uint8_t>()>;
		const std::vector<std::uint8_t>& ReadPayload(const AssetEntry& entry, const PayloadProvider& provider);
		const AssetData& LoadRaw(const AssetEntry& entry, const PayloadProvider& provider);
		void ClearCache();

	private:
		std::unordered_map<std::string, AssetData> m_cache;
		std::optional<std::pair<std::string, std::vector<std::uint8_t>>> m_lastPayload;
	};
}
