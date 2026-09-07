#pragma once

#include "assets/asset_archive.hpp"

#include <functional>
#include <memory>
#include <unordered_map>

namespace LeagueModel::Assets
{
	class AssetArchiveSet
	{
	public:
		using ProgressCallback = std::function<void(size_t completed, size_t total)>;

		void Mount(std::shared_ptr<AssetArchive> archive);
		void ClearMounts();
		std::vector<AssetEntry> Scan(const ProgressCallback& onProgress = {});
		std::vector<AssetEntry> Entries() const;
		const AssetEntry* GetEntry(const std::string& path) const;
		const AssetEntry* FindByName(const std::string& name, const std::string& nearDirectory = {}) const;
		std::vector<std::uint8_t> ReadPayload(const AssetEntry& entry) const;
		const std::vector<std::pair<std::string, std::string>>& ScanErrors() const { return m_scanErrors; }

	private:
		std::vector<std::shared_ptr<AssetArchive>> m_archives;
		std::unordered_map<std::string, AssetEntry> m_entries;
		std::unordered_map<std::string, std::shared_ptr<AssetArchive>> m_entryArchives;
		std::unordered_map<std::string, std::string> m_baseNameIndex;
		std::vector<std::pair<std::string, std::string>> m_scanErrors;
	};
}
