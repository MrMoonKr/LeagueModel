#pragma once

#include "assets/asset_archive_set.hpp"
#include "assets/asset_registry.hpp"
#include "assets/game_hash_index.hpp"
#include "assets/load_record.hpp"

#include <filesystem>

namespace LeagueModel::Assets
{
	class AssetSystem
	{
	public:
		void Mount(std::shared_ptr<AssetArchive> archive) { m_archives.Mount(std::move(archive)); }
		void MountDirectory(const std::filesystem::path& root, const std::string& archiveId = {});
		void MountWad(const std::filesystem::path& filePath, const GameHashIndex* hashIndex = nullptr, const std::string& archiveId = {});
		std::vector<AssetEntry> Open(const AssetArchiveSet::ProgressCallback& onProgress = {});
		std::vector<AssetEntry> ListEntries() const { return m_archives.Entries(); }
		const AssetEntry* GetEntry(const std::string& path) const { return m_archives.GetEntry(path); }
		const AssetEntry* FindByName(const std::string& name, const std::string& nearDirectory = {}) const { return m_archives.FindByName(name, nearDirectory); }
		const AssetData& InspectRaw(const AssetEntry& entry, std::string parentId = {});
		const std::vector<std::uint8_t>& ReadPayload(const AssetEntry& entry, std::string parentId = {});
		void MarkPartial(const AssetEntry& entry, std::string detail, std::string parentId = {});
		const std::vector<std::pair<std::string, std::string>>& ScanErrors() const { return m_archives.ScanErrors(); }
		const std::vector<LoadRecord>& LoadRecords() const { return m_loadRecords; }
		void ClearLoadRecords() { m_loadRecords.clear(); }

	private:
		AssetArchiveSet m_archives;
		AssetRegistry m_registry;
		std::vector<LoadRecord> m_loadRecords;
	};
}
