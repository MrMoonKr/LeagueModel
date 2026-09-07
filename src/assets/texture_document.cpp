#include "assets/texture_document.hpp"
#include "assets/binary_reader.hpp"

#include <cstring>
#include <stdexcept>

namespace LeagueModel::Assets
{
	bool TextureDocument::Load(std::span<const std::uint8_t> data, std::string* error)
	{
		if (data.size() >= 4 && std::memcmp(data.data(), "DDS ", 4) == 0)
		{
			kind = Kind::Dds; payload = data;
			return true;
		}
		if (data.size() < 12 || std::memcmp(data.data(), "TEX\0", 4) != 0)
		{
			if (error) *error = "Expected DDS or TEX header";
			return false;
		}
		try
		{
			BinaryReader reader(data);
			reader.Read(4);
			width = reader.ReadU16(); height = reader.ReadU16();
			reader.ReadU8(); format = reader.ReadU8(); reader.ReadU8(); hasMipmaps = reader.ReadU8() != 0;
			if (width == 0 || height == 0) throw std::runtime_error("Texture dimensions are zero");
			if (format != 0x0a && format != 0x0c && format != 0x14) throw std::runtime_error("Unsupported TEX format " + std::to_string(format));
			kind = Kind::Tex; payload = data;
			return true;
		}
		catch (const std::exception& exception) { if (error) *error = exception.what(); return false; }
	}
}
