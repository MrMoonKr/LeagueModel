#pragma once

#include "assets/asset_types.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace LeagueModel::Assets
{
	class AssetArchiveError : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	class AssetArchive
	{
	public:
		virtual ~AssetArchive() = default;
		virtual const std::string& ArchiveId() const = 0;
		virtual std::vector<AssetEntry> Scan() = 0;
		virtual std::vector<AssetEntry> Entries() const = 0;
		virtual std::vector<std::uint8_t> ReadPayload(const AssetEntry& entry) const = 0;
	};
}
