#include "assets/skeleton_document.hpp"
#include "assets/binary_reader.hpp"

#include <stdexcept>

namespace LeagueModel::Assets
{
	bool SkeletonDocument::Load(std::span<const std::uint8_t> payload, std::string* error)
	{
		try
		{
			BinaryReader reader(payload);
			reader.ReadU32(); // signature: variants in the wild are not semantically used.
			type = static_cast<Type>(reader.ReadU32());
			version = reader.ReadU32();
			bones.clear(); boneIndices.clear(); hierarchyComplete = true;
			if (type == Type::Classic)
			{
				reader.ReadU32();
				const auto count = reader.ReadU32();
				bones.reserve(count);
				for (std::uint32_t index = 0; index < count; ++index)
				{
					SkeletonBoneInfo bone;
					bone.name = reader.ReadFixedString(32);
					bone.parentId = reader.ReadI16(); reader.ReadI16();
					reader.ReadF32(); // classic scale is unused by the existing renderer
					for (float& value : bone.matrix3x4) value = reader.ReadF32();
					bones.push_back(std::move(bone));
				}
				if (version < 2)
					for (std::uint32_t index = 0; index < count; ++index) boneIndices.push_back(index);
				else if (version == 2)
				{
					const auto countIndices = reader.ReadU32();
					for (std::uint32_t index = 0; index < countIndices; ++index) boneIndices.push_back(reader.ReadU32());
				}
			}
			else if (type == Type::Version2)
			{
				reader.ReadU16();
				const auto count = reader.ReadU16();
				const auto countIndices = reader.ReadU32();
				const auto dataOffset = reader.ReadU16();
				reader.ReadU16();
				reader.ReadU32(); // unused bone index map offset
				const auto indicesOffset = reader.ReadU32();
				reader.Read(8);
				const auto namesOffset = reader.ReadU32();
				if (dataOffset > payload.size() || indicesOffset > payload.size() || namesOffset > payload.size()) throw std::runtime_error("SKL v2 offset outside payload");
				reader.Seek(dataOffset);
				bones.reserve(count);
				for (std::uint32_t index = 0; index < count; ++index)
				{
					reader.ReadU16(); reader.ReadI16();
					SkeletonBoneInfo bone;
					bone.parentId = reader.ReadI16(); reader.ReadU16();
					bone.hash = reader.ReadU32();
					reader.Read(4);
					for (float& value : bone.position) value = reader.ReadF32();
					for (float& value : bone.scale) value = reader.ReadF32();
					for (float& value : bone.rotation) value = reader.ReadF32();
					bone.usesLocalTransform = true;
					reader.Read(44);
					bones.push_back(std::move(bone));
				}
				reader.Seek(indicesOffset);
				for (std::uint32_t index = 0; index < countIndices; ++index) boneIndices.push_back(reader.ReadU16());
				reader.Seek(namesOffset);
				for (auto& bone : bones) bone.name = reader.ReadCString();
			}
			else throw std::runtime_error("Unsupported SKL type");
			for (const auto& bone : bones)
				// Legacy SKL variants can use values that are not a dense parent index.
				if (bone.parentId >= 0 && static_cast<size_t>(bone.parentId) >= bones.size()) hierarchyComplete = false;
			return true;
		}
		catch (const std::exception& exception)
		{
			bones.clear(); boneIndices.clear(); hierarchyComplete = false;
			if (error) *error = exception.what();
			return false;
		}
	}
}
