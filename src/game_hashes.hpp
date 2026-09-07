#pragma once

#include <cstdint>
#include <string>

namespace LeagueModel
{
	// Starts loading cache/hashes.game.txt (the CommunityDragon hash -> asset path
	// list) on a background thread. Safe to call multiple times; only the first
	// call starts the load.
	void RequestGameHashes();

	// True once the load started by RequestGameHashes() has finished (whether or
	// not the file was found).
	bool AreGameHashesReady();

	// Resolves an XXHash64 asset path hash to its original path, or nullptr if the
	// table isn't loaded yet or the hash isn't known.
	const std::string* LookupGameHash(uint64_t inHash);
}
