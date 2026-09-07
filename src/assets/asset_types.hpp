#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace LeagueModel::Assets
{
	struct AssetPath
	{
		std::optional<std::string> archiveName;
		std::string path;

		std::string ToString() const;
		static std::string Normalize(std::string path);
	};

	struct AssetEntry
	{
		AssetPath path;
		std::uint64_t fileOffset = 0;
		std::uint64_t packedSize = 0;
		std::uint64_t fileSize = 0;
		std::optional<std::uint64_t> hash;

		std::string Id() const;
	};

	struct AssetData
	{
		AssetEntry entry;
		std::string kind;
		std::vector<std::uint8_t> payload;
	};
}
