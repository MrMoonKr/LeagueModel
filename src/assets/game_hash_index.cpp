#include "assets/game_hash_index.hpp"

#include <fstream>

namespace LeagueModel::Assets
{
	bool GameHashIndex::Load(const std::filesystem::path& filePath, std::string* error)
	{
		std::ifstream file(filePath);
		if (!file)
		{
			if (error) *error = "Cannot open hash index: " + filePath.string();
			return false;
		}
		m_paths.clear();
		m_hashes.clear();
		std::string line;
		while (std::getline(file, line))
		{
			if (line.size() < 18 || line[16] != ' ') continue;
			try
			{
				const std::uint64_t hash = std::stoull(line.substr(0, 16), nullptr, 16);
				std::string path = line.substr(17);
				if (!path.empty() && path.back() == '\r') path.pop_back();
				if (!path.empty())
				{
					m_hashes.emplace(path, hash);
					m_paths.emplace(hash, std::move(path));
				}
			}
			catch (const std::exception&) {}
		}
		return true;
	}

	const std::string* GameHashIndex::Lookup(std::uint64_t hash) const
	{
		const auto found = m_paths.find(hash);
		return found == m_paths.end() ? nullptr : &found->second;
	}

	const std::uint64_t* GameHashIndex::LookupHash(const std::string& path) const
	{
		const auto found = m_hashes.find(path);
		return found == m_hashes.end() ? nullptr : &found->second;
	}
}
