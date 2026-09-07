#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace LeagueModel::Assets
{
	struct SkeletonBoneInfo
	{
		std::string name;
		std::uint32_t hash = 0;
		std::int16_t parentId = -1;
		float matrix3x4[12]{}; // Classic global matrix, in source order.
		float position[3]{};
		float scale[3]{ 1.0f, 1.0f, 1.0f };
		float rotation[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
		bool usesLocalTransform = false;
	};

	struct SkeletonDocument
	{
		enum class Type : std::uint32_t { Classic = 0x746C6B73, Version2 = 0x22FD4FC3 };
		Type type = Type::Classic;
		std::uint32_t version = 0;
		bool hierarchyComplete = true;
		std::vector<SkeletonBoneInfo> bones;
		std::vector<std::uint32_t> boneIndices;

		bool Load(std::span<const std::uint8_t> payload, std::string* error = nullptr);
	};
}
