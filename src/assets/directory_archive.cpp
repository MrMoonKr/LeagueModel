#include "assets/directory_archive.hpp"

#include <algorithm>
#include <fstream>

namespace LeagueModel::Assets
{
	DirectoryArchive::DirectoryArchive(std::filesystem::path root, std::string archiveId) :
		m_root(std::move(root)),
		m_archiveId(archiveId.empty() ? "directory:" + m_root.filename().string() : std::move(archiveId))
	{
	}

	std::vector<AssetEntry> DirectoryArchive::Scan()
	{
		if (!std::filesystem::is_directory(m_root))
			throw AssetArchiveError("Directory not found: " + m_root.string());

		m_entries.clear();
		for (const auto& item : std::filesystem::recursive_directory_iterator(m_root))
		{
			if (!item.is_regular_file())
				continue;
			const std::string path = AssetPath::Normalize(std::filesystem::relative(item.path(), m_root).generic_string());
			const std::uint64_t size = item.file_size();
			m_entries.emplace(path, AssetEntry{ { m_archiveId, path }, 0, size, size, std::nullopt });
		}
		return Entries();
	}

	std::vector<AssetEntry> DirectoryArchive::Entries() const
	{
		std::vector<AssetEntry> result;
		result.reserve(m_entries.size());
		for (const auto& [_, entry] : m_entries) result.push_back(entry);
		std::sort(result.begin(), result.end(), [](const AssetEntry& left, const AssetEntry& right) { return left.path.path < right.path.path; });
		return result;
	}

	std::vector<std::uint8_t> DirectoryArchive::ReadPayload(const AssetEntry& entry) const
	{
		if (!entry.path.archiveName || *entry.path.archiveName != m_archiveId)
			throw AssetArchiveError("Entry does not belong to " + m_archiveId + ": " + entry.path.ToString());
		const auto found = m_entries.find(AssetPath::Normalize(entry.path.path));
		if (found == m_entries.end() || found->second.Id() != entry.Id())
			throw AssetArchiveError("Unknown directory entry: " + entry.path.ToString());

		const std::filesystem::path filePath = (m_root / found->second.path.path).lexically_normal();
		const std::filesystem::path rootPath = std::filesystem::absolute(m_root).lexically_normal();
		const std::filesystem::path absoluteFilePath = std::filesystem::absolute(filePath).lexically_normal();
		const auto rootText = rootPath.generic_string();
		const auto fileText = absoluteFilePath.generic_string();
		if (fileText.compare(0, rootText.size(), rootText) != 0 || (fileText.size() > rootText.size() && fileText[rootText.size()] != '/'))
			throw AssetArchiveError("Directory entry escapes root: " + entry.path.path);

		std::ifstream file(absoluteFilePath, std::ios::binary | std::ios::ate);
		if (!file) throw AssetArchiveError("Cannot open directory entry: " + entry.path.ToString());
		const std::streamsize size = file.tellg();
		if (size < 0) throw AssetArchiveError("Cannot determine entry size: " + entry.path.ToString());
		std::vector<std::uint8_t> payload(static_cast<size_t>(size));
		file.seekg(0);
		if (!payload.empty() && !file.read(reinterpret_cast<char*>(payload.data()), size))
			throw AssetArchiveError("Cannot read directory entry: " + entry.path.ToString());
		return payload;
	}
}
