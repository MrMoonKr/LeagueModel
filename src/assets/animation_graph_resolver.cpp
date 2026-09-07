#include "assets/animation_graph_resolver.hpp"

namespace LeagueModel::Assets
{
	namespace
	{
		const BinValue* Field(const BinObject* object, const char* name) { return object ? object->Find(BinFieldHash(name)) : nullptr; }
		const BinMap* MapField(const BinObject* object, const char* name) { const BinValue* value = Field(object, name); return value ? value->Map() : nullptr; }
		std::string ResolvePath(const BinValue* value, const GameHashIndex& index)
		{
			if (!value) return {};
			if (const auto text = value->As<std::string>()) return *text;
			if (const auto hash = value->As<std::uint64_t>()) if (const auto path = index.Lookup(*hash)) return *path;
			return {};
		}
	}

	bool ResolveAnimationGraphAssets(const BinDocument& document, const std::string& characterName, std::uint8_t skinIndex,
		const GameHashIndex& hashIndex, AnimationGraphAssets& result, std::string* error)
	{
		result = {};
		const std::string rootName = "Characters/" + characterName + "/Animations/Skin" + std::to_string(skinIndex);
		const BinObject* root = document.DecodeRoot(BinFieldHash(rootName), error);
		if (!root) return false;
		const BinMap* clips = MapField(root, "mClipDataMap");
		if (!clips) { if (error) *error = "mClipDataMap is missing"; return false; }
		result.clipCount = clips->values.size();
		if (const auto masks = MapField(root, "mMaskDataMap")) result.maskCount = masks->values.size();
		if (const auto tracks = MapField(root, "mTrackDataMap")) result.trackCount = tracks->values.size();
		for (const auto& [key, value] : clips->values)
		{
			const auto hash = key.As<std::uint32_t>();
			const BinObject* clip = value.Object();
			const BinObject* resource = Field(clip, "mAnimationResourceData") ? Field(clip, "mAnimationResourceData")->Object() : nullptr;
			const std::string path = ResolvePath(Field(resource, "mAnimationFilePath"), hashIndex);
			if (hash && !path.empty()) result.clipResources[*hash] = path;
		}
		return true;
	}
}
