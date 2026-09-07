#include "assets/asset_system.hpp"
#include "assets/binary_reader.hpp"
#include "assets/skin_document.hpp"
#include "assets/skeleton_document.hpp"
#include "assets/texture_document.hpp"
#include "assets/anm_document.hpp"

#include <cassert>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace LeagueModel::Assets;
namespace fs = std::filesystem;

int main()
{
	const std::vector<std::uint8_t> binary = { 0x34, 0x12, 0x00, 'o', 'k', 0 };
	BinaryReader reader(binary);
	assert(reader.ReadU16() == 0x1234);
	assert(reader.ReadCString() == "");
	assert(reader.ReadCString() == "ok");
	bool rejectedOutOfBoundsRead = false;
	try { reader.ReadU8(); }
	catch (const std::out_of_range&) { rejectedOutOfBoundsRead = true; }
	assert(rejectedOutOfBoundsRead);
	std::vector<std::uint8_t> skn;
	auto appendU16 = [&skn](std::uint16_t value) { skn.push_back(static_cast<std::uint8_t>(value)); skn.push_back(static_cast<std::uint8_t>(value >> 8)); };
	auto appendU32 = [&skn](std::uint32_t value) { for (int i = 0; i < 4; ++i) skn.push_back(static_cast<std::uint8_t>(value >> (i * 8))); };
	appendU32(0x00112233); appendU16(0); appendU16(0); appendU32(0); // v0: no submesh table
	appendU32(3); appendU32(1); // three 16-bit indices, one 52-byte vertex
	skn.insert(skn.end(), 6, 0); skn.insert(skn.end(), 52, 0);
	SkinDocument skin;
	std::string skinError;
	assert(skin.Load(skn, &skinError));
	assert(skin.indexCount == 3 && skin.vertexCount == 1 && skin.indices.size() == 3 && skin.vertices.size() == 1 && skin.indexData.size() == 6 && skin.vertexData.size() == 52);
	std::vector<std::uint8_t> skl;
	auto appendSkl16 = [&skl](std::uint16_t value) { skl.push_back(static_cast<std::uint8_t>(value)); skl.push_back(static_cast<std::uint8_t>(value >> 8)); };
	auto appendSkl32 = [&skl](std::uint32_t value) { for (int i = 0; i < 4; ++i) skl.push_back(static_cast<std::uint8_t>(value >> (i * 8))); };
	appendSkl32(0); appendSkl32(0x746C6B73); appendSkl32(1); appendSkl32(0); appendSkl32(1);
	skl.insert(skl.end(), {'r', 'o', 'o', 't'}); skl.insert(skl.end(), 28, 0); appendSkl16(0xffff); appendSkl16(0); skl.insert(skl.end(), 52, 0);
	SkeletonDocument skeleton;
	std::string skeletonError;
	assert(skeleton.Load(skl, &skeletonError));
	assert(skeleton.bones.size() == 1 && skeleton.bones.front().name == "root" && skeleton.bones.front().parentId == -1);
	const std::vector<std::uint8_t> tex = { 'T', 'E', 'X', 0, 0x40, 0, 0x20, 0, 0, 0x14, 0, 1 };
	TextureDocument texture;
	std::string textureError;
	assert(texture.Load(tex, &textureError));
	assert(texture.kind == TextureDocument::Kind::Tex && texture.width == 64 && texture.height == 32 && texture.hasMipmaps);
	std::vector<std::uint8_t> anmV1(44, 0);
	const char anmSignature[] = "r3d2anmd";
	std::copy(anmSignature, anmSignature + 8, anmV1.begin());
	anmV1[8] = 1;
	AnmDocument animationDocument;
	std::string animationError;
	assert(animationDocument.Load(anmV1, &animationError));
	assert(animationDocument.version == 1 && animationDocument.metadataComplete);

	const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	const fs::path root = fs::temp_directory_path() / ("LeagueModelAssetTests-" + unique);
	const fs::path base = root / "base";
	const fs::path override = root / "override";
	fs::create_directories(base / "data");
	fs::create_directories(override / "data");
	{ std::ofstream(base / "data/item.bin", std::ios::binary) << "base"; }
	{ std::ofstream(override / "data/item.bin", std::ios::binary) << "override"; }
	{ std::ofstream(base / "data/other.bin", std::ios::binary) << "other"; }

	AssetSystem assets;
	assets.MountDirectory(base, "base");
	assets.MountDirectory(override, "override");
	assets.Open();
	const AssetEntry* item = assets.GetEntry("data\\item.bin");
	assert(item != nullptr && item->path.archiveName && *item->path.archiveName == "override");
	const auto& itemPayload = assets.ReadPayload(*item);
	assert(std::string(itemPayload.begin(), itemPayload.end()) == "override");
	assert(assets.LoadRecords().size() == 1);
	assert(assets.LoadRecords().front().state == AssetLoadState::Loaded);
	assert(assets.LoadRecords().front().entry.Id() == item->Id());
	assets.MarkPartial(*item, "Unsupported test payload", "skin0.bin");
	assert(assets.LoadRecords().back().state == AssetLoadState::Partial);
	assert(assets.LoadRecords().back().parentId == "skin0.bin");
	AssetEntry missing;
	missing.path.archiveName = "override";
	missing.path.path = "data/missing.bin";
	bool missingReadRejected = false;
	try { assets.ReadPayload(missing, item->Id()); }
	catch (const AssetArchiveError&) { missingReadRejected = true; }
	assert(missingReadRejected);
	assert(assets.LoadRecords().back().state == AssetLoadState::Failed);
	assert(assets.LoadRecords().back().parentId == item->Id());
	assert(assets.FindByName("other.bin") != nullptr);

	fs::remove_all(root);
	return 0;
}
