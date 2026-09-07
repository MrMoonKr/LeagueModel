#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace LeagueModel::Assets
{
	struct TextureDocument
	{
		enum class Kind { Dds, Tex };
		Kind kind = Kind::Tex;
		std::uint16_t width = 0;
		std::uint16_t height = 0;
		std::uint8_t format = 0;
		bool hasMipmaps = false;
		std::span<const std::uint8_t> payload;

		bool Load(std::span<const std::uint8_t> data, std::string* error = nullptr);
	};
}
