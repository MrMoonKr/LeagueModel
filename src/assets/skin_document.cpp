#include "assets/skin_document.hpp"
#include "assets/binary_reader.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace LeagueModel::Assets
{
	bool SkinDocument::Load(std::span<const std::uint8_t> payload, std::string* error)
	{
		try
		{
			BinaryReader reader(payload);
			if (reader.ReadU32() != 0x00112233) throw std::runtime_error("Invalid SKN signature");
			majorVersion = reader.ReadU16();
			minorVersion = reader.ReadU16();
			if (majorVersion > 4) throw std::runtime_error("Unsupported SKN version " + std::to_string(majorVersion));
			const auto meshCount = reader.ReadU32();
			meshes.clear();
			if (majorVersion > 0)
			{
				for (std::uint32_t i = 0; i < meshCount; ++i)
				{
					auto nameBytes = reader.Read(64);
					const auto end = std::find(nameBytes.begin(), nameBytes.end(), std::uint8_t{});
					SkinMeshRange mesh;
					mesh.name.assign(reinterpret_cast<const char*>(nameBytes.data()), static_cast<size_t>(end - nameBytes.begin()));
					mesh.vertexOffset = reader.ReadU32();
					mesh.vertexCount = reader.ReadU32();
					mesh.indexOffset = reader.ReadU32();
					mesh.indexCount = reader.ReadU32();
					meshes.push_back(std::move(mesh));
				}
			}
			if (majorVersion == 4) reader.Read(4);
			indexCount = reader.ReadU32();
			vertexCount = reader.ReadU32();
			std::uint32_t vertexSize = 52;
			if (majorVersion == 4)
			{
				vertexSize = reader.ReadU32();
				const auto hasColour = reader.ReadU32();
				if (vertexSize < 52 || (hasColour != 0 && vertexSize < 56)) throw std::runtime_error("Invalid SKN vertex layout");
				reader.Read(10 * sizeof(float)); // bounds plus bounding sphere
			}
			const size_t indexBytes = static_cast<size_t>(indexCount) * sizeof(std::uint16_t);
			indexData = reader.Read(indexBytes);
			indices.resize(indexCount);
			for (size_t index = 0; index < indices.size(); ++index)
				std::memcpy(&indices[index], indexData.data() + index * sizeof(std::uint16_t), sizeof(std::uint16_t));
			const size_t vertexBytes = static_cast<size_t>(vertexCount) * vertexSize;
			vertexData = reader.Read(vertexBytes);
			vertices.clear();
			vertices.reserve(vertexCount);
			BinaryReader vertexReader(vertexData);
			for (std::uint32_t index = 0; index < vertexCount; ++index)
			{
				SkinVertex vertex;
				for (float& value : vertex.position) value = vertexReader.ReadF32();
				for (std::uint8_t& value : vertex.boneIndices) value = vertexReader.ReadU8();
				for (float& value : vertex.weights) value = vertexReader.ReadF32();
				for (float& value : vertex.normal) value = vertexReader.ReadF32();
				for (float& value : vertex.uv) value = vertexReader.ReadF32();
				vertexReader.Read(vertexSize - 52); // colour and any future trailing fields
				vertices.push_back(vertex);
			}
			for (const auto& mesh : meshes)
				if (static_cast<std::uint64_t>(mesh.vertexOffset) + mesh.vertexCount > vertexCount || static_cast<std::uint64_t>(mesh.indexOffset) + mesh.indexCount > indexCount)
					throw std::runtime_error("SKN submesh range is outside the payload");
			return true;
		}
		catch (const std::exception& exception)
		{
			meshes.clear(); indices.clear(); vertices.clear(); indexData = {}; vertexData = {}; vertexCount = 0; indexCount = 0;
			if (error) *error = exception.what();
			return false;
		}
	}
}
