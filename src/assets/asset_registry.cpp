#include "assets/asset_registry.hpp"

namespace LeagueModel::Assets
{
	const std::vector<std::uint8_t>& AssetRegistry::ReadPayload(const AssetEntry& entry, const PayloadProvider& provider)
	{
		const std::string id = entry.Id();
		if (!m_lastPayload || m_lastPayload->first != id)
			m_lastPayload.emplace(id, provider());
		return m_lastPayload->second;
	}

	const AssetData& AssetRegistry::LoadRaw(const AssetEntry& entry, const PayloadProvider& provider)
	{
		const std::string id = entry.Id();
		const auto cached = m_cache.find(id);
		if (cached != m_cache.end()) return cached->second;
		AssetData data{ entry, "raw", ReadPayload(entry, provider) };
		return m_cache.emplace(id, std::move(data)).first->second;
	}

	void AssetRegistry::ClearCache()
	{
		m_cache.clear();
		m_lastPayload.reset();
	}
}
