#include "assets/asset_system.hpp"
#include "assets/directory_archive.hpp"
#include "assets/wad_archive.hpp"

namespace LeagueModel::Assets
{
	void AssetSystem::MountDirectory(const std::filesystem::path& root, const std::string& archiveId)
	{
		Mount(std::make_shared<DirectoryArchive>(root, archiveId));
	}

	void AssetSystem::MountWad(const std::filesystem::path& filePath, const GameHashIndex* hashIndex, const std::string& archiveId)
	{
		Mount(std::make_shared<WadArchive>(filePath, hashIndex, archiveId));
	}

	std::vector<AssetEntry> AssetSystem::Open(const AssetArchiveSet::ProgressCallback& onProgress)
	{
		m_registry.ClearCache();
		m_loadRecords.clear();
		return m_archives.Scan(onProgress);
	}

	const AssetData& AssetSystem::InspectRaw(const AssetEntry& entry, std::string parentId)
	{
		m_loadRecords.push_back({ entry, AssetLoadState::Loading, std::move(parentId) });
		try
		{
			const AssetData& data = m_registry.LoadRaw(entry, [this, &entry] { return m_archives.ReadPayload(entry); });
			m_loadRecords.back().state = AssetLoadState::Loaded;
			return data;
		}
		catch (const std::exception& error)
		{
			m_loadRecords.back().state = AssetLoadState::Failed;
			m_loadRecords.back().error = error.what();
			throw;
		}
	}

	const std::vector<std::uint8_t>& AssetSystem::ReadPayload(const AssetEntry& entry, std::string parentId)
	{
		m_loadRecords.push_back({ entry, AssetLoadState::Loading, std::move(parentId) });
		try
		{
			const auto& data = m_registry.ReadPayload(entry, [this, &entry] { return m_archives.ReadPayload(entry); });
			m_loadRecords.back().state = AssetLoadState::Loaded;
			return data;
		}
		catch (const std::exception& error)
		{
			m_loadRecords.back().state = AssetLoadState::Failed;
			m_loadRecords.back().error = error.what();
			throw;
		}
	}

	void AssetSystem::MarkPartial(const AssetEntry& entry, std::string detail, std::string parentId)
	{
		m_loadRecords.push_back({ entry, AssetLoadState::Partial, std::move(parentId), std::move(detail) });
	}
}
