#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace LeagueModel::Assets
{
	struct SkinMeshRange
	{
		std::string name;
		std::uint32_t vertexOffset = 0;
		std::uint32_t vertexCount = 0;
		std::uint32_t indexOffset = 0;
		std::uint32_t indexCount = 0;
	};
	struct SkinVertex
	{
		float position[3]{};
		std::uint8_t boneIndices[4]{};
		float weights[4]{};
		float normal[3]{};
		float uv[2]{};
	};

	// Native, ownership-free SKN decoder. Rendering conversion remains separate.
	struct SkinDocument
	{
		std::uint16_t majorVersion = 0;
		std::uint16_t minorVersion = 0;
		std::uint32_t vertexCount = 0;
		std::uint32_t indexCount = 0;
		std::vector<SkinMeshRange> meshes;
		std::vector<std::uint16_t> indices;
		std::vector<SkinVertex> vertices;
		std::span<const std::uint8_t> indexData;
		std::span<const std::uint8_t> vertexData;

		bool Load(std::span<const std::uint8_t> payload, std::string* error = nullptr);
	};
}
