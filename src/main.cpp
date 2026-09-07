#include "app/league_model_app.hpp"
#include "assets/wad_archive.hpp"
#include "assets/game_hash_index.hpp"
#include "assets/bin_document.hpp"
#include "assets/character_asset_resolver.hpp"
#include "assets/animation_graph_resolver.hpp"
#include "assets/skin_document.hpp"
#include "assets/skeleton_document.hpp"
#include "skin.hpp"
#include "skeleton.hpp"
#include "assets/texture_document.hpp"
#include "assets/static_character_loader.hpp"
#include "assets/anm_document.hpp"
#include "animation.hpp"
#include <league_lib/wad/wad_filesystem.hpp>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <stdexcept>

int main(int argc, char** argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	if (argc == 3 && std::strcmp(argv[1], "--check-character-static") == 0)
	{
		try
		{
			LeagueModel::Assets::GameHashIndex hashIndex;
			std::string error;
			if (!hashIndex.Load("cache/hashes.game.txt", &error)) throw std::runtime_error(error);
			LeagueModel::Assets::AssetSystem assets;
			assets.MountWad(argv[2], &hashIndex, "Jinx");
			assets.Open();
			LeagueModel::Assets::StaticCharacterAssets character;
			if (!LeagueModel::Assets::LoadStaticCharacterAssets(assets, "Jinx", 0, hashIndex, character, &error)) throw std::runtime_error(error);
			printf("STATIC_CHARACTER_CHECK vertices=%zu bones=%zu texture=%ux%u submesh_textures=%zu records=%zu\n", character.skin.vertices.size(), character.skeleton.bones.size(), character.texture.width, character.texture.height, character.submeshTextures.size(), assets.LoadRecords().size());
			return 0;
		}
		catch (const std::exception& error) { printf("STATIC_CHARACTER_CHECK failed: %s\n", error.what()); return 2; }
	}
	if (argc == 4 && std::strcmp(argv[1], "--check-static-new") == 0)
	{
		try
		{
			LeagueModel::Assets::GameHashIndex hashIndex;
			std::string error;
			if (!hashIndex.Load("cache/hashes.game.txt", &error)) throw std::runtime_error(error);
			LeagueModel::Assets::WadArchive wad(argv[2], &hashIndex);
			const auto entries = wad.Scan();
			const auto entry = std::find_if(entries.begin(), entries.end(), [path = std::string(argv[3])](const auto& candidate) { return candidate.path.path == path; });
			if (entry == entries.end()) throw std::runtime_error("Static asset path is not in this WAD");
			const auto payload = wad.ReadPayload(*entry);
			const std::string path = argv[3];
			if (path.ends_with(".skn"))
			{
				LeagueModel::Assets::SkinDocument skin;
				if (!skin.Load(payload, &error)) throw std::runtime_error(error);
				LeagueModel::Skin renderSkin;
				if (!renderSkin.LoadPayload(payload, &error)) throw std::runtime_error(error);
				printf("STATIC_NEW_CHECK skn version=%u.%u meshes=%zu vertices=%u indices=%u render_vertices=%zu\n", skin.majorVersion, skin.minorVersion, skin.meshes.size(), skin.vertexCount, skin.indexCount, renderSkin.vertices.size());
			}
			else if (path.ends_with(".skl"))
			{
				LeagueModel::Assets::SkeletonDocument skeleton;
				if (!skeleton.Load(payload, &error)) throw std::runtime_error(error);
				LeagueModel::Skeleton renderSkeleton;
				if (!renderSkeleton.LoadPayload(payload, &error)) throw std::runtime_error(error);
				printf("STATIC_NEW_CHECK skl version=%u bones=%zu indices=%zu hierarchy_complete=%d render_bones=%zu\n", skeleton.version, skeleton.bones.size(), skeleton.boneIndices.size(), skeleton.hierarchyComplete, renderSkeleton.bones.size());
			}
			else if (path.ends_with(".tex") || path.ends_with(".dds"))
			{
				LeagueModel::Assets::TextureDocument texture;
				if (!texture.Load(payload, &error)) throw std::runtime_error(error);
				printf("STATIC_NEW_CHECK texture kind=%s width=%u height=%u format=%02X mipmaps=%d\n", texture.kind == LeagueModel::Assets::TextureDocument::Kind::Dds ? "dds" : "tex", texture.width, texture.height, texture.format, texture.hasMipmaps);
			}
			else if (path.ends_with(".anm"))
			{
				LeagueModel::Assets::AnmDocument animation;
				if (!animation.Load(payload, &error)) throw std::runtime_error(error);
				LeagueModel::Animation playable;
				if (!playable.LoadPayload(payload, &error)) throw std::runtime_error(error);
				printf("STATIC_NEW_CHECK anm version=%u metadata_complete=%d bones=%u keys=%u playable_bones=%zu fps=%.2f duration=%.2f\n", animation.version, animation.metadataComplete, animation.boneCount, animation.keyframeCount, playable.bones.size(), playable.fps, playable.duration);
			}
			else throw std::runtime_error("Expected .skn, .skl, .tex, .dds or .anm path");
			return 0;
		}
		catch (const std::exception& error) { printf("STATIC_NEW_CHECK failed: %s\n", error.what()); return 2; }
	}
	if (argc == 4 && std::strcmp(argv[1], "--check-graph-new") == 0)
	{
		try
		{
			LeagueModel::Assets::GameHashIndex hashIndex;
			std::string error;
			if (!hashIndex.Load("cache/hashes.game.txt", &error)) throw std::runtime_error(error);
			LeagueModel::Assets::WadArchive wad(argv[2], &hashIndex);
			const auto entries = wad.Scan();
			const auto entry = std::find_if(entries.begin(), entries.end(), [path = std::string(argv[3])](const auto& candidate) { return candidate.path.path == path; });
			if (entry == entries.end()) throw std::runtime_error("Animation graph path is not in this WAD");
			LeagueModel::Assets::BinDocument graph;
			if (!graph.Load(wad.ReadPayload(*entry), &error)) throw std::runtime_error(error);
			LeagueModel::Assets::AnimationGraphAssets assets;
			if (!LeagueModel::Assets::ResolveAnimationGraphAssets(graph, "Jinx", 0, hashIndex, assets, &error)) throw std::runtime_error(error);
			printf("GRAPH_NEW_CHECK clips=%zu resources=%zu masks=%zu tracks=%zu\n", assets.clipCount, assets.clipResources.size(), assets.maskCount, assets.trackCount);
			for (const auto& [hash, path] : assets.clipResources) printf("GRAPH_ANM %08X %s\n", hash, path.c_str());
			return assets.clipResources.empty() ? 2 : 0;
		}
		catch (const std::exception& error) { printf("GRAPH_NEW_CHECK failed: %s\n", error.what()); return 2; }
	}
	if (argc == 4 && std::strcmp(argv[1], "--check-bin-new") == 0)
	{
		try
		{
			LeagueModel::Assets::GameHashIndex hashIndex;
			std::string error;
			if (!hashIndex.Load("cache/hashes.game.txt", &error)) throw std::runtime_error(error);
			LeagueModel::Assets::WadArchive wad(argv[2], &hashIndex);
			const auto entries = wad.Scan();
			const auto entry = std::find_if(entries.begin(), entries.end(), [path = std::string(argv[3])](const auto& candidate) { return candidate.path.path == path; });
			if (entry == entries.end()) throw std::runtime_error("BIN path is not in this WAD");
			LeagueModel::Assets::BinDocument bin;
			if (!bin.Load(wad.ReadPayload(*entry), &error)) throw std::runtime_error(error);
			const auto rootHash = LeagueModel::Assets::BinFieldHash("Characters/Jinx/Skins/Skin0");
			const auto root = bin.DecodeRoot(rootHash, &error);
			LeagueModel::Assets::SkinAssetReferences references;
			const bool resolved = LeagueModel::Assets::ResolveSkinAssets(bin, "Jinx", 0, hashIndex, references, &error);
			const auto meshProperties = root ? root->Find(LeagueModel::Assets::BinFieldHash("skinMeshProperties")) : nullptr;
			const auto meshObject = meshProperties ? meshProperties->Object() : nullptr;
			printf("BIN_NEW_CHECK version=%u links=%zu root=%d fields=%zu meshFields=%zu error=%s\n", bin.Version(), bin.LinkedFiles().size(), root != nullptr, root ? root->fields.size() : 0, meshObject ? meshObject->fields.size() : 0, error.c_str());
			if (meshObject)
				for (const auto& [fieldHash, value] : meshObject->fields)
					if (const auto text = value.As<std::string>()) printf("BIN_FIELD %08X text=%s\n", fieldHash, text->c_str());
					else if (const auto path = value.As<std::uint64_t>()) printf("BIN_FIELD %08X path=%016llx\n", fieldHash, static_cast<unsigned long long>(*path));
			printf("BIN_ASSETS resolved=%d skn=%s skl=%s tex=%s graph=%08X\n", resolved, references.skinMeshPath.c_str(), references.skeletonPath.c_str(), references.texturePath.c_str(), references.animationGraphHash.value_or(0));
			for (const auto& link : bin.LinkedFiles()) printf("BIN_LINK %s\n", link.c_str());
			return root ? 0 : 2;
		}
		catch (const std::exception& error) { printf("BIN_NEW_CHECK failed: %s\n", error.what()); return 2; }
	}
	if (argc == 3 && std::strcmp(argv[1], "--check-wad") == 0)
	{
		try
		{
			LeagueModel::Assets::GameHashIndex hashIndex;
			std::string hashIndexError;
			if (!hashIndex.Load("cache/hashes.game.txt", &hashIndexError))
				printf("WAD_CHECK hash index unavailable: %s\n", hashIndexError.c_str());
			LeagueModel::Assets::WadArchive wad(argv[2], hashIndex.Size() ? &hashIndex : nullptr);
			const auto entries = wad.Scan();
			if (entries.empty())
			{
				printf("WAD_CHECK entries=0\n");
				return 2;
			}
			const auto payload = wad.ReadPayload(entries.front());
			printf("WAD_CHECK entries=%zu first=%s bytes=%zu\n", entries.size(), entries.front().path.path.c_str(), payload.size());
			return 0;
		}
		catch (const std::exception& error)
		{
			printf("WAD_CHECK failed: %s\n", error.what());
			return 2;
		}
	}
	if (argc == 4 && std::strcmp(argv[1], "--check-bin") == 0)
	{
		Spek::File::Mount<LeagueLib::WADFileSystem>(argv[2]);
		LeagueLib::Bin bin;
		bool done = false;
		bool ok = false;
		const std::string champion = argv[3];
		bin.Load("data/characters/" + champion + "/skins/skin0.bin", [&](LeagueLib::Bin& loaded)
		{
			const auto& root = loaded["Characters/" + champion + "/Skins/Skin0"];
			ok = root.IsValid() && loaded.GetLoadState() == Spek::File::LoadState::Loaded;
			printf("BIN_CHECK %s state=%d root=%d error=%s\n", champion.c_str(), int(loaded.GetLoadState()), root.IsValid(), loaded.GetLastError().c_str());
			for (const auto& link : loaded.GetLinkedFiles()) printf("LINK %s\n", link.c_str());
			done = true;
		});
		for (int i = 0; !done && i < 10000; ++i) Spek::File::Update();
		return ok ? 0 : 2;
	}
	LeagueModel::LeagueModelApp app(argc, argv);
	if (!app.InitInstance())
		return -1;

	const int exitCode = app.Run();
	app.ExitInstance();
	return exitCode;
}
