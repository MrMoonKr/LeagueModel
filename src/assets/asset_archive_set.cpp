#include "assets/asset_archive_set.hpp"

#include <algorithm>

namespace LeagueModel::Assets
{
	void AssetArchiveSet::Mount(std::shared_ptr<AssetArchive> archive)
	{
		if (!archive) throw AssetArchiveError("Cannot mount a null archive");
		for (const auto& mounted : m_archives)
			if (mounted->ArchiveId() == archive->ArchiveId())
				throw AssetArchiveError("Archive already mounted: " + archive->ArchiveId());
		m_archives.push_back(std::move(archive));
	}

	void AssetArchiveSet::ClearMounts()
	{
		m_archives.clear();
		m_entries.clear();
		m_entryArchives.clear();
		m_baseNameIndex.clear();
		m_scanErrors.clear();
	}

	std::vector<AssetEntry> AssetArchiveSet::Scan(const ProgressCallback& onProgress)
	{
		if (m_archives.empty()) throw AssetArchiveError("No asset archives are mounted");
		m_entries.clear();
		m_entryArchives.clear();
		m_baseNameIndex.clear();
		m_scanErrors.clear();
		for (size_t index = 0; index < m_archives.size(); ++index)
		{
			const auto& archive = m_archives[index];
			try
			{
				for (AssetEntry entry : archive->Scan())
				{
					entry.path.path = AssetPath::Normalize(entry.path.path);
					m_entries[entry.path.path] = entry; // Last mounted archive wins.
					m_entryArchives[entry.Id()] = archive;
				}
			}
			catch (const AssetArchiveError& error)
			{
				m_scanErrors.emplace_back(archive->ArchiveId(), error.what());
			}
			if (onProgress) onProgress(index + 1, m_archives.size());
		}
		for (const auto& [path, _] : m_entries)
		{
			const size_t slash = path.find_last_of('/');
			const std::string baseName = path.substr(slash == std::string::npos ? 0 : slash + 1);
			m_baseNameIndex.try_emplace(baseName, path);
		}
		return Entries();
	}

	std::vector<AssetEntry> AssetArchiveSet::Entries() const
	{
		std::vector<AssetEntry> result;
		result.reserve(m_entries.size());
		for (const auto& [_, entry] : m_entries) result.push_back(entry);
		std::sort(result.begin(), result.end(), [](const AssetEntry& left, const AssetEntry& right) { return left.path.path < right.path.path; });
		return result;
	}

	const AssetEntry* AssetArchiveSet::GetEntry(const std::string& path) const
	{
		const auto found = m_entries.find(AssetPath::Normalize(path));
		return found == m_entries.end() ? nullptr : &found->second;
	}

	const AssetEntry* AssetArchiveSet::FindByName(const std::string& name, const std::string& nearDirectory) const
	{
		const std::string normalizedName = AssetPath::Normalize(name);
		if (!nearDirectory.empty())
			if (const AssetEntry* nearby = GetEntry(AssetPath::Normalize(nearDirectory) + "/" + normalizedName)) return nearby;
		const size_t slash = normalizedName.find_last_of('/');
		const std::string baseName = normalizedName.substr(slash == std::string::npos ? 0 : slash + 1);
		const auto found = m_baseNameIndex.find(baseName);
		return found == m_baseNameIndex.end() ? nullptr : GetEntry(found->second);
	}

	std::vector<std::uint8_t> AssetArchiveSet::ReadPayload(const AssetEntry& entry) const
	{
		const auto found = m_entryArchives.find(entry.Id());
		if (found == m_entryArchives.end()) throw AssetArchiveError("Entry has no mounted archive: " + entry.path.ToString());
		return found->second->ReadPayload(entry);
	}
}
