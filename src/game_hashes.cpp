#include "game_hashes.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unordered_map>

namespace LeagueModel
{
	namespace fs = std::filesystem;

	static struct
	{
		std::unordered_map<uint64_t, std::string> table;
		std::atomic<bool> started{ false };
		std::atomic<bool> ready{ false };
		std::thread thread;
	} g_gameHashes;

	static const fs::path gameHashesFile = fs::current_path() / "cache" / "hashes.game.txt";

	static void LoadGameHashesThread()
	{
		std::ifstream file(gameHashesFile, std::ios::binary);
		if (file.is_open())
		{
			std::string line;
			while (std::getline(file, line))
			{
				// Each line is "<16 hex chars> <path>".
				if (line.size() < 18 || line[16] != ' ')
					continue;

				uint64_t hash = std::strtoull(line.substr(0, 16).c_str(), nullptr, 16);

				std::string path = line.substr(17);
				if (!path.empty() && path.back() == '\r')
					path.pop_back();

				g_gameHashes.table.emplace(hash, std::move(path));
			}
		}
		else
		{
			printf("Failed to open %s, animation/asset names will be shown as hashes.\n", gameHashesFile.string().c_str());
		}

		g_gameHashes.ready = true;
	}

	void RequestGameHashes()
	{
		if (g_gameHashes.started.exchange(true))
			return;

		g_gameHashes.thread = std::thread(LoadGameHashesThread);

		std::atexit([]()
		{
			if (g_gameHashes.thread.joinable())
				g_gameHashes.thread.join();
		});
	}

	bool AreGameHashesReady()
	{
		return g_gameHashes.ready;
	}

	const std::string* LookupGameHash(uint64_t inHash)
	{
		if (!g_gameHashes.ready)
			return nullptr;

		auto index = g_gameHashes.table.find(inHash);
		return index != g_gameHashes.table.end() ? &index->second : nullptr;
	}
}
