#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace LeagueModel::Assets
{
	struct AnmDocument
	{
		std::string signature;
		std::uint32_t version = 0;
		std::uint32_t boneCount = 0;
		std::uint32_t keyframeCount = 0;
		float duration = 0.0f;
		float fps = 0.0f;
		bool metadataComplete = false;

		bool Load(std::span<const std::uint8_t> payload, std::string* error = nullptr);
	};
}
