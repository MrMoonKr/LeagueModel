#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace LeagueModel::Assets
{
	class GameHashIndex
	{
	public:
		bool Load(const std::filesystem::path& filePath, std::string* error = nullptr);
		const std::string* Lookup(std::uint64_t hash) const;
		const std::uint64_t* LookupHash(const std::string& path) const;
		size_t Size() const { return m_paths.size(); }

	private:
		std::unordered_map<std::uint64_t, std::string> m_paths;
		std::unordered_map<std::string, std::uint64_t> m_hashes;
	};
}
