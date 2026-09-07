#include "assets/character_asset_resolver.hpp"

namespace LeagueModel::Assets
{
	namespace
	{
		const BinValue* Field(const BinObject* object, const char* name) { return object ? object->Find(BinFieldHash(name)) : nullptr; }
		std::string AssetPath(const BinValue* value, const GameHashIndex& index)
		{
			if (!value) return {};
			if (const auto text = value->As<std::string>()) return *text;
			if (const auto hash = value->As<std::uint64_t>()) { if (const auto resolved = index.Lookup(*hash)) return *resolved; }
			return {};
		}
		std::string DiffuseTexture(const BinObject* material, const GameHashIndex& index)
		{
			const BinArray* samplers = Field(material, "samplerValues") ? Field(material, "samplerValues")->Array() : nullptr;
			if (!samplers) return {};
			for (const BinValue& sampler : samplers->values)
			{
				const BinObject* object = sampler.Object();
				const BinValue* name = Field(object, "samplerName");
				const auto text = name ? name->As<std::string>() : nullptr;
				if (!text || (text->find("Diffuse") == std::string::npos && text->find("diffuse") == std::string::npos)) continue;
				if (const std::string path = AssetPath(Field(object, "textureName"), index); !path.empty()) return path;
			}
			return {};
		}
	}

	bool ResolveSkinAssets(const BinDocument& document, const std::string& characterName, std::uint8_t skinIndex,
		const GameHashIndex& hashIndex, SkinAssetReferences& result, std::string* error)
	{
		result = {};
		const std::string rootName = "Characters/" + characterName + "/Skins/Skin" + std::to_string(skinIndex);
		const BinObject* root = document.DecodeRoot(BinFieldHash(rootName), error);
		if (!root) return false;
		const BinObject* mesh = Field(root, "skinMeshProperties") ? Field(root, "skinMeshProperties")->Object() : nullptr;
		if (!mesh) { if (error) *error = "skinMeshProperties is missing"; return false; }
		result.skinMeshPath = AssetPath(Field(mesh, "simpleSkin"), hashIndex);
		result.skeletonPath = AssetPath(Field(mesh, "skeleton"), hashIndex);
		result.texturePath = AssetPath(Field(mesh, "texture"), hashIndex);
		if (const BinValue* overrides = Field(mesh, "materialOverride"))
			if (const BinArray* entries = overrides->Array())
				for (const BinValue& entry : entries->values)
				{
					const BinObject* override = entry.Object();
					const std::string submesh = AssetPath(Field(override, "submesh"), hashIndex);
					if (submesh.empty()) continue;
					std::string texture = AssetPath(Field(override, "texture"), hashIndex);
					if (texture.empty())
						if (const BinValue* materialHash = Field(override, "material"))
							if (const auto hash = materialHash->As<std::uint32_t>()) texture = DiffuseTexture(document.DecodeRoot(*hash), hashIndex);
					if (!texture.empty()) result.submeshTexturePaths[BinFieldHash(submesh)] = texture;
				}
		if (const BinValue* properties = Field(root, "skinAnimationProperties"))
			if (const BinObject* animation = properties->Object())
				if (const auto graph = Field(animation, "animationGraphData"); graph && graph->As<std::uint32_t>()) result.animationGraphHash = *graph->As<std::uint32_t>();
		if (result.skinMeshPath.empty() || result.skeletonPath.empty()) { if (error) *error = "skin mesh or skeleton path is missing"; return false; }
		return true;
	}
}
