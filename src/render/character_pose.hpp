#pragma once

#include <array>

#include <glm/mat4x4.hpp>

namespace LeagueModel
{
	constexpr size_t kMaxShaderBones = 255;

	struct CharacterPose
	{
		std::array<glm::mat4, kMaxShaderBones> bones = {};
	};
}
