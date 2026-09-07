#include "assets/anm_document.hpp"
#include "assets/binary_reader.hpp"

#include <cstring>
#include <stdexcept>

namespace LeagueModel::Assets
{
	bool AnmDocument::Load(std::span<const std::uint8_t> payload, std::string* error)
	{
		try
		{
			BinaryReader reader(payload);
			signature.assign(reinterpret_cast<const char*>(reader.Read(8).data()), 8);
			if (signature != "r3d2anmd" && signature != "r3d2canm") throw std::runtime_error("Unknown ANM signature");
			version = reader.ReadU32();
			if (version != 1 && version != 3 && version != 4 && version != 5) throw std::runtime_error("Unsupported ANM version " + std::to_string(version));
			metadataComplete = false;
			if (version == 1)
			{
				reader.ReadU32(); reader.Read(8);
				boneCount = reader.ReadU32();
				keyframeCount = reader.ReadU32(); reader.Read(4);
				duration = reader.ReadF32(); fps = reader.ReadF32();
				metadataComplete = true;
			}
			return true;
		}
		catch (const std::exception& exception) { if (error) *error = exception.what(); return false; }
	}
}
