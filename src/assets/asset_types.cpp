#include "assets/asset_types.hpp"

#include <algorithm>
#include <cctype>

namespace LeagueModel::Assets
{
	std::string AssetPath::ToString() const
	{
		return (archiveName ? *archiveName : "local") + ":" + path;
	}

	std::string AssetPath::Normalize(std::string value)
	{
		std::replace(value.begin(), value.end(), '\\', '/');
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		while (!value.empty() && value.front() == '/') value.erase(value.begin());
		while (!value.empty() && value.back() == '/') value.pop_back();
		return value;
	}

	std::string AssetEntry::Id() const
	{
		return path.ToString() + ":" + std::to_string(fileOffset);
	}
}
