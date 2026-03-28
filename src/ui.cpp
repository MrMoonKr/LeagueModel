#include "ui.hpp"
#include "character.hpp"
#include "managed_image.hpp"

#include <string>
#include <cassert>
#include <thread>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <imgui.h>
#include <json.hpp>

#if defined(_WIN32) || defined(_WIN64)
	#include <Windows.h>
	#include <wininet.h>
	#pragma comment(lib, "wininet.lib")
#elif defined(__EMSCRIPTEN__)
	#include <emscripten/fetch.h>
#endif

namespace LeagueModel
{
	namespace fs = std::filesystem;
	static struct
	{
		bool initialisationStarted = false;
		bool initialised = false;

		nlohmann::json champions;
		bool skinsWindowOpen = true;
		char skinsWindowFilter[256] = { 0 };

		bool animationWindowOpen = true;
		char animationWindowFilter[256] = { 0 };

		std::thread thread;
	} g_ui;

	static const fs::path cachePath = fs::current_path() / "cache";
	static const fs::path championCacheFile = cachePath / "champions.json";

	struct LoadingProgress
	{
		bool active = false;
		float progress = 0.0f;
		std::string phase;
	};

	static bool IsCharacterLoadStarted(const Character& character)
	{
		return character.loadState != CharacterLoadState::NotLoaded ||
			   character.globalTexture != nullptr ||
			   !character.textures.empty() ||
			   character.skin.loadState != Spek::File::LoadState::NotLoaded ||
			   character.skeleton.state != Spek::File::LoadState::NotLoaded;
	}

	static bool IsFlagSet(const Character& character, CharacterLoadState state)
	{
		return (character.loadState & state) != 0;
	}

	static LoadingProgress GetCharacterLoadingProgress(const Character& character)
	{
		LoadingProgress result = {};
		if (!IsCharacterLoadStarted(character))
			return result;

		if ((character.loadState & CharacterLoadState::FailedBitSet) != 0)
		{
			result.active = true;
			result.progress = 1.0f;
			result.phase = "Load failed";
			return result;
		}

		float completedWeight = 0.0f;
		float totalWeight = 0.0f;
		auto addStep = [&](bool done, float weight)
		{
			totalWeight += weight;
			if (done)
				completedWeight += weight;
		};

		addStep(IsFlagSet(character, CharacterLoadState::SkinLoaded) || IsFlagSet(character, CharacterLoadState::SkinFailed), 1.0f);
		addStep(IsFlagSet(character, CharacterLoadState::SkeletonLoaded) || IsFlagSet(character, CharacterLoadState::SkeletonFailed), 1.0f);
		addStep(IsFlagSet(character, CharacterLoadState::GraphLoaded) || IsFlagSet(character, CharacterLoadState::GraphFailed), 1.0f);
		addStep(IsFlagSet(character, CharacterLoadState::SkeletonApplied), 0.75f);
		addStep(IsFlagSet(character, CharacterLoadState::MaterialApplied), 0.75f);
		addStep(IsFlagSet(character, CharacterLoadState::InfoLoadCompleted), 1.0f);
		addStep(IsFlagSet(character, CharacterLoadState::MeshGenCompleted), 1.5f);
		addStep(IsFlagSet(character, CharacterLoadState::CallbackCompleted), 0.5f);

		size_t textureCount = character.textures.size() + (character.globalTexture ? 1 : 0);
		if (textureCount > 0)
		{
			size_t resolvedTextureCount = 0;
			auto isTextureResolved = [](const std::shared_ptr<ManagedImage>& image)
			{
				return image == nullptr || image->loadState != Spek::File::LoadState::NotLoaded;
			};

			if (isTextureResolved(character.globalTexture))
				resolvedTextureCount++;

			for (const auto& texture : character.textures)
				if (isTextureResolved(texture.second))
					resolvedTextureCount++;

			const float textureWeight = 1.5f;
			totalWeight += textureWeight;
			completedWeight += textureWeight * (static_cast<float>(resolvedTextureCount) / static_cast<float>(textureCount));
		}

		result.active = (character.loadState & CharacterLoadState::Loaded) != CharacterLoadState::Loaded;
		result.progress = totalWeight > 0.0f ? std::clamp(completedWeight / totalWeight, 0.0f, 1.0f) : 0.0f;

		if (!IsFlagSet(character, CharacterLoadState::SkinLoaded) && !IsFlagSet(character, CharacterLoadState::SkinFailed))
			result.phase = "Loading skin";
		else if (!IsFlagSet(character, CharacterLoadState::SkeletonLoaded) && !IsFlagSet(character, CharacterLoadState::SkeletonFailed))
			result.phase = "Loading skeleton";
		else if (!IsFlagSet(character, CharacterLoadState::GraphLoaded) && !IsFlagSet(character, CharacterLoadState::GraphFailed))
			result.phase = "Loading animation graph";
		else if (!IsFlagSet(character, CharacterLoadState::SkeletonApplied))
			result.phase = "Applying skeleton";
		else if (!IsFlagSet(character, CharacterLoadState::MaterialApplied))
			result.phase = "Applying materials";
		else if (!IsFlagSet(character, CharacterLoadState::InfoLoadCompleted))
			result.phase = "Preparing mesh data";
		else if (!IsFlagSet(character, CharacterLoadState::MeshGenCompleted))
			result.phase = "Uploading mesh";
		else if (!IsFlagSet(character, CharacterLoadState::CallbackCompleted))
			result.phase = "Finalizing";
		else
			result.phase = "Ready";

		return result;
	}

	static void RenderLoadingOverlay(const Character& character)
	{
		const LoadingProgress characterProgress = GetCharacterLoadingProgress(character);
		const bool showChampionLoading = !g_ui.initialised;
		if (!showChampionLoading && !characterProgress.active)
			return;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.9f);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
								 ImGuiWindowFlags_NoDocking |
								 ImGuiWindowFlags_NoMove |
								 ImGuiWindowFlags_NoSavedSettings |
								 ImGuiWindowFlags_NoNav;
		if (ImGui::Begin("LoadingOverlay", nullptr, flags))
		{
			if (characterProgress.active)
			{
				const int percent = static_cast<int>(characterProgress.progress * 100.0f + 0.5f);
				ImGui::TextUnformatted(characterProgress.phase.c_str());
				ImGui::ProgressBar(characterProgress.progress, ImVec2(-1.0f, 0.0f));
				ImGui::Text("Character loading: %d%%", percent);
			}

			if (showChampionLoading)
			{
				if (characterProgress.active)
					ImGui::Separator();
				ImGui::TextUnformatted("Loading champion list...");
			}
		}
		ImGui::End();
	}

	static void RequestChampionList()
	{
	#if defined(_WIN32) || defined(_WIN64)
		if (!fs::exists(cachePath))
			fs::create_directory(cachePath);

		if (fs::exists(championCacheFile))
		{
			std::ifstream file(championCacheFile);
			if (file.is_open())
			{
				std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
				g_ui.champions = nlohmann::json::parse(content);
				g_ui.initialised = true;
				return;
			}
		}

		g_ui.thread = std::thread([]()
		{
			std::string url = "https://cdn.merakianalytics.com/riot/lol/resources/latest/en-US/champions.json";

			HINTERNET client = InternetOpenA("LeagueModel", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
			if (client == nullptr)
			{
				assert(false);
				return;
			}

			HINTERNET	request = InternetOpenUrlA(client, url.c_str(), nullptr, 0, INTERNET_FLAG_RELOAD, 0);
			if (request == nullptr)
			{
				assert(false);
				InternetCloseHandle(client);
				return;
			}

			std::string responseBody;
			char		buffer[1024];
			DWORD		bytesRead;
			while (InternetReadFile(request, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
				responseBody.append(buffer, bytesRead);

			InternetCloseHandle(request);
			InternetCloseHandle(client);

			std::ofstream file(championCacheFile);
			if (file.is_open())
				file << responseBody;
			file.close();

			g_ui.champions = nlohmann::json::parse(responseBody);
			g_ui.initialised = true;
		});

		std::atexit([]()
		{
			if (g_ui.thread.joinable())
				g_ui.thread.join();
		});

	#elif defined(__EMSCRIPTEN__)
		std::string url = "https://cdn.merakianalytics.com/riot/lol/resources/latest/en-US/champions.json";

		emscripten_fetch_attr_t attr;
		emscripten_fetch_attr_init(&attr);
		strcpy(attr.requestMethod, "GET");
		attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
		attr.onsuccess = [](emscripten_fetch_t* fetch)
		{

			g_ui.champions = nlohmann::json::parse(fetch->data);
			g_ui.initialised = true;
		};

		attr.onerror = = [](emscripten_fetch_t* fetch)
		{
			printf("Downloading %s failed, HTTP failure status code: %d.\n", fetch->url, fetch->status);
		};

		emscripten_fetch(&attr, url.c_str());
	#endif
	}

	void RenderSkinsWindow(Character& character)
	{
		if (g_ui.skinsWindowOpen && ImGui::Begin("Skins", &g_ui.skinsWindowOpen))
		{
			ImGui::InputText("Filter", g_ui.skinsWindowFilter, sizeof(g_ui.skinsWindowFilter));
			if (ImGui::SmallButton("Clear Filter"))
				g_ui.skinsWindowFilter[0] = '\0';
			ImGui::Separator();

			for (auto& championKV : g_ui.champions.items())
			{
				const std::string& championName = championKV.key();
				const auto& champion = championKV.value();

				// Check if matches filter
				bool shouldFilter = false;
				if (g_ui.skinsWindowFilter[0] != '\0')
				{
					if (championName.find(g_ui.skinsWindowFilter) == std::string::npos)
						shouldFilter = true;

					for (auto& skinKV : champion["skins"].items())
					{
						auto& skin = skinKV.value();
						std::string skinName = skin["name"].get<std::string>() + " " + championName;
						if (skinName.find(g_ui.skinsWindowFilter) != std::string::npos)
						{
							shouldFilter = false;
							break;
						}
					}
				}

				if (shouldFilter)
					continue;

				if (ImGui::CollapsingHeader(championName.c_str()))
				{
					ImGui::Indent();
					for (auto& skinKV : champion["skins"].items())
					{
						auto& skin = skinKV.value();
						std::string skinName = skin["name"].get<std::string>() + " " + championName;
						if (g_ui.skinsWindowFilter[0] != '\0' && skinName.find(g_ui.skinsWindowFilter) == std::string::npos)
							continue;

						if (ImGui::Button(skinName.c_str()))
						{
							i32 championId = champion["id"].get<i32>();
							u8 skinId = skin["id"].get<i32>() - championId * 1000;
							character.Load(championName.c_str(), skinId);
						}
					}
					ImGui::Unindent();
				}
			}

			ImGui::End();
		}
	}

	void RenderAnimationsWindow(Character& character)
	{
		if (g_ui.skinsWindowOpen && ImGui::Begin("Animations", &g_ui.skinsWindowOpen))
		{
			ImGui::InputText("Filter", g_ui.animationWindowFilter, sizeof(g_ui.animationWindowFilter));
			if (ImGui::SmallButton("Clear Filter"))
				g_ui.animationWindowFilter[0] = '\0';
			ImGui::Separator();

			for (auto& animationKV : character.animations)
			{
				const std::string& animationName = animationKV.first;
				auto& animation = *animationKV.second;

				// Check if matches filter
				if (g_ui.animationWindowFilter[0] != '\0' && animationName.find(g_ui.animationWindowFilter) == std::string::npos)
					continue;

				const char* nameStart = strrchr(animationName.c_str(), '/') + 1;
				assert((u64)nameStart > 1);
				if (ImGui::Button(nameStart))
					character.PlayAnimation(animation);
			}

			ImGui::End();
		}
	}

	void RenderUI(Character& character)
	{
		if (!g_ui.initialised && !g_ui.initialisationStarted)
		{
			RequestChampionList();

			g_ui.initialisationStarted = true;
		}
		const LoadingProgress characterProgress = GetCharacterLoadingProgress(character);

		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Windows"))
			{
				ImGui::MenuItem("Skins", nullptr, &g_ui.skinsWindowOpen);
				ImGui::EndMenu();
			}

			if (characterProgress.active)
			{
				ImGui::Separator();
				ImGui::Text("Loading %d%%", static_cast<int>(characterProgress.progress * 100.0f + 0.5f));
			}
			else if (!g_ui.initialised)
			{
				ImGui::Separator();
				ImGui::TextUnformatted("Loading champion list...");
			}

			ImGui::TextColored(ImVec4(1, 1, 1, 0.4), "FPS: %.1f", ImGui::GetIO().Framerate);
			ImGui::EndMainMenuBar();
		}

		if (g_ui.initialised)
		{
			RenderSkinsWindow(character);
			RenderAnimationsWindow(character);
		}

		RenderLoadingOverlay(character);
	}
}
