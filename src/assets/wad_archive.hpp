#pragma once

#include "assets/asset_archive.hpp"
#include "assets/game_hash_index.hpp"

#include <filesystem>
#include <unordered_map>

namespace LeagueModel::Assets
{
	class WadArchive final : public AssetArchive
	{
	public:
		explicit WadArchive(std::filesystem::path filePath, const GameHashIndex* hashIndex = nullptr, std::string archiveId = {});

		const std::string& ArchiveId() const override { return m_archiveId; }
		std::vector<AssetEntry> Scan() override;
		std::vector<AssetEntry> Entries() const override;
		std::vector<std::uint8_t> ReadPayload(const AssetEntry& entry) const override;

	private:
		struct FileRecord
		{
			std::uint64_t hash = 0;
			std::uint32_t offset = 0;
			std::uint32_t packedSize = 0;
			std::uint32_t fileSize = 0;
			std::uint8_t typeData = 0;
			std::uint16_t firstSubchunkIndex = 0;
		};

		std::vector<std::uint8_t> ReadFileRange(std::uint64_t offset, std::uint64_t size) const;
		std::filesystem::path m_filePath;
		std::string m_archiveId;
		const GameHashIndex* m_hashIndex = nullptr;
		std::unordered_map<std::uint64_t, FileRecord> m_records;
		std::unordered_map<std::string, AssetEntry> m_entries;
	};
}
