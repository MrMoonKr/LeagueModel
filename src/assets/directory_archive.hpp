#pragma once

#include "assets/asset_archive.hpp"

#include <filesystem>
#include <unordered_map>

namespace LeagueModel::Assets
{
	class DirectoryArchive final : public AssetArchive
	{
	public:
		explicit DirectoryArchive(std::filesystem::path root, std::string archiveId = {});

		const std::string& ArchiveId() const override { return m_archiveId; }
		std::vector<AssetEntry> Scan() override;
		std::vector<AssetEntry> Entries() const override;
		std::vector<std::uint8_t> ReadPayload(const AssetEntry& entry) const override;

	private:
		std::filesystem::path m_root;
		std::string m_archiveId;
		std::unordered_map<std::string, AssetEntry> m_entries;
	};
}
