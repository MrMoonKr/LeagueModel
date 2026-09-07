#include "assets/static_character_loader.hpp"

#include <algorithm>
#include <cctype>

namespace LeagueModel::Assets
{
	bool LoadStaticCharacterAssets(AssetSystem& assets, const std::string& characterName, std::uint8_t skinIndex,
		const GameHashIndex& hashes, StaticCharacterAssets& result, std::string* error)
	{
		std::string pathName = characterName;
		std::transform(pathName.begin(), pathName.end(), pathName.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
		const std::string binPath = "data/characters/" + pathName + "/skins/skin" + std::to_string(skinIndex) + ".bin";
		const AssetEntry* binEntry = assets.GetEntry(binPath);
		if (!binEntry) { if (error) *error = "Skin BIN is not mounted: " + binPath; return false; }
		const AssetData& binData = assets.InspectRaw(*binEntry);
		BinDocument bin;
		std::string detail;
		if (!bin.Load(binData.payload, &detail)) { if (error) *error = "BIN header: " + detail; return false; }
		if (!ResolveSkinAssets(bin, characterName, skinIndex, hashes, result.references, &detail)) { if (error) *error = "Skin references: " + detail; return false; }

		auto read = [&assets, &binEntry, &detail](const std::string& path, auto& document, const char* label) -> bool
		{
			const AssetEntry* entry = assets.GetEntry(path);
			if (!entry) { detail = std::string(label) + " is not mounted: " + path; return false; }
			const auto& payload = assets.ReadPayload(*entry, binEntry->Id());
			if (!document.Load(payload, &detail)) { detail = std::string(label) + ": " + detail; return false; }
			return true;
		};
		if (!read(result.references.skinMeshPath, result.skin, "SKN") || !read(result.references.skeletonPath, result.skeleton, "SKL") || !read(result.references.texturePath, result.texture, "Texture"))
		{
			if (error) *error = detail;
			return false;
		}
		for (const auto& [submeshHash, texturePath] : result.references.submeshTexturePaths)
		{
			if (texturePath == result.references.texturePath) continue;
			const AssetEntry* entry = assets.GetEntry(texturePath);
			if (!entry)
			{
				AssetEntry missing; missing.path.path = texturePath;
				assets.MarkPartial(missing, "Submesh texture is not mounted", binEntry->Id());
				continue;
			}
			TextureDocument texture;
			std::string textureError;
			if (!texture.Load(assets.ReadPayload(*entry, binEntry->Id()), &textureError))
			{
				assets.MarkPartial(*entry, "Submesh texture: " + textureError, binEntry->Id());
				continue;
			}
			result.submeshTextures.emplace(submeshHash, std::move(texture));
		}
		return true;
	}
}
