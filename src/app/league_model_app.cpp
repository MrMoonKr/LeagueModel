#include "app/league_model_app.hpp"

#include "league_lib/wad/wad_filesystem.hpp"
#include "ui.hpp"
#include "assets/game_hash_index.hpp"
#include "assets/bin_document.hpp"
#include "assets/character_asset_resolver.hpp"
#include "assets/animation_graph_resolver.hpp"
#include "assets/skin_document.hpp"
#include "assets/skeleton_document.hpp"
#include "assets/texture_document.hpp"
#include "assets/static_character_loader.hpp"
#include "assets/anm_document.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

namespace LeagueModel
{
	namespace fs = std::filesystem;

	namespace
	{
		LeagueLib::WADFileSystem* MountDir(const char* wadPath)
		{
			if (wadPath == nullptr || !fs::exists(wadPath))
				return nullptr;

			const fs::path rootPath = wadPath;
			if (fs::is_directory(rootPath / "Champions"))
				return Spek::File::Mount<LeagueLib::WADFileSystem>((rootPath / "Champions").generic_string().c_str());
			if (rootPath.filename() == "Champions")
				return Spek::File::Mount<LeagueLib::WADFileSystem>(rootPath.generic_string().c_str());
			if (fs::is_regular_file(rootPath) && rootPath.filename() == "DATA.wad.client")
			{
				return Spek::File::Mount<LeagueLib::WADFileSystem>(rootPath.parent_path().string().c_str());
			}

			const fs::path directWadPath = rootPath / "DATA.wad.client";
			if (fs::exists(directWadPath) && fs::is_regular_file(directWadPath))
			{
				return Spek::File::Mount<LeagueLib::WADFileSystem>(rootPath.string().c_str());
			}

			for (auto& path : fs::recursive_directory_iterator(rootPath))
			{
				if (!path.is_regular_file())
					continue;

				if (path.path().filename() == "DATA.wad.client")
				{
					return Spek::File::Mount<LeagueLib::WADFileSystem>(path.path().parent_path().string().c_str());
				}
			}

			return nullptr;
		}

		std::string Trim(const std::string& value)
		{
			const size_t start = value.find_first_not_of(" \t\r\n");
			if (start == std::string::npos)
				return "";

			const size_t end = value.find_last_not_of(" \t\r\n");
			return value.substr(start, end - start + 1);
		}

		std::string Unquote(const std::string& value)
		{
			if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
				return value.substr(1, value.size() - 2);

			return value;
		}

		std::string ReadConfigRoot(const fs::path& configPath)
		{
			std::ifstream configFile(configPath);
			if (!configFile.is_open())
				return "";

			std::string line;
			while (std::getline(configFile, line))
			{
				const std::string trimmedLine = Trim(line);
				if (trimmedLine.empty() || trimmedLine[0] == '#' || trimmedLine[0] == ';')
					continue;

				const size_t delimiter = trimmedLine.find('=');
				if (delimiter == std::string::npos)
					continue;

				const std::string key = Trim(trimmedLine.substr(0, delimiter));
				if (key != "root")
					continue;

				return Unquote(Trim(trimmedLine.substr(delimiter + 1)));
			}

			return "";
		}

		std::string ResolveGameRoot(int argc, char** argv)
		{
			if (argc > 1 && argv[1] != nullptr && fs::exists(argv[1]))
				return argv[1];

			constexpr const char* defaultRoot = "C:/Riot Games/League of Legends/Game/DATA/FINAL";
			std::vector<fs::path> configPaths;
			if (argc > 0 && argv[0] != nullptr)
				configPaths.push_back(fs::path(argv[0]).parent_path() / "config.ini");

			const fs::path workingConfig = fs::current_path() / "config.ini";
			if (configPaths.empty() || configPaths.front() != workingConfig)
				configPaths.push_back(workingConfig);

			for (const fs::path& configPath : configPaths)
			{
				const std::string configuredRoot = ReadConfigRoot(configPath);
				if (!configuredRoot.empty() && fs::exists(configuredRoot))
					return configuredRoot;
			}

			return defaultRoot;
		}
	}

	LeagueModelApp::LeagueModelApp(int argc, char** argv) :
		m_argc(argc),
		m_argv(argv)
	{
	}

	WindowConfig LeagueModelApp::GetWindowConfig() const
	{
		return { 1600, 900, "LeagueModel - Jinx - Loading" };
	}

	bool LeagueModelApp::OnInit()
	{
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		// Preserve the winding convention used by the previous sokol renderer.
		glFrontFace(GL_CW);
		glCullFace(GL_FRONT);

		if (!m_renderer.Initialize())
			return false;

		m_gameRootPath = ResolveGameRoot(m_argc, m_argv);
		MountConfiguredRoots();
		InitializeNativeAssetPreview();
		m_camera.SetDistance(400.0f);
		return true;
	}

	void LeagueModelApp::MountConfiguredRoots()
	{
		if (!m_gameRootPath.empty() && fs::exists(m_gameRootPath))
			m_wadFileSystem = MountDir(m_gameRootPath.c_str());

		const fs::path localDirectory = fs::current_path();
		if (m_wadFileSystem == nullptr && fs::exists(localDirectory))
			m_wadFileSystem = MountDir(localDirectory.string().c_str());
	}

	void LeagueModelApp::InitializeNativeAssetPreview()
	{
		if (m_gameRootPath.empty())
			return;

		std::string error;
		m_nativeHashIndex.Load(fs::current_path() / "cache" / "hashes.game.txt", &error);
		const fs::path jinxWad = fs::path(m_gameRootPath) / "Champions" / "Jinx.wad.client";
		if (!fs::is_regular_file(jinxWad))
			return;

		try
		{
			m_nativeAssets.MountWad(jinxWad, m_nativeHashIndex.Size() ? &m_nativeHashIndex : nullptr, "Jinx");
			m_nativeAssets.Open();
			Assets::StaticCharacterAssets staticAssets;
			if (!Assets::LoadStaticCharacterAssets(m_nativeAssets, "Jinx", 0, m_nativeHashIndex, staticAssets, &error))
			{
				if (const auto* entry = m_nativeAssets.GetEntry("data/characters/jinx/skins/skin0.bin"))
					m_nativeAssets.MarkPartial(*entry, "Static asset pipeline: " + error);
			}
			const auto* idleAnimation = m_nativeAssets.GetEntry("assets/characters/jinx/skins/base/animations/jinx_minigun_idle1.anm");
			if (idleAnimation != nullptr)
			{
				const auto& payload = m_nativeAssets.ReadPayload(*idleAnimation);
				Assets::AnmDocument animation;
				std::string animationError;
				if (!animation.Load(payload, &animationError))
					m_nativeAssets.MarkPartial(*idleAnimation, "ANM decode: " + animationError);
				else if (!animation.metadataComplete && animation.version != 4)
					m_nativeAssets.MarkPartial(*idleAnimation, "ANM keyframe metadata is not yet supported for version " + std::to_string(animation.version));
				if (animation.version == 4)
				{
					auto playable = std::make_shared<Animation>();
					if (playable->LoadPayload(payload, &animationError))
					{
						playable->name = "[Native] Jinx Minigun Idle";
						playable->sourcePath = idleAnimation->path.path;
						m_nativeAnimationPreview = std::move(playable);
					}
					else m_nativeAssets.MarkPartial(*idleAnimation, "ANM playback conversion: " + animationError);
				}
			}
			const auto* skinBin = m_nativeAssets.GetEntry("data/characters/jinx/skins/skin0.bin");
			if (skinBin == nullptr)
				return;

			const auto& rawBin = m_nativeAssets.InspectRaw(*skinBin);
			Assets::BinDocument skinDocument;
			if (!skinDocument.Load(rawBin.payload, &error))
			{
				m_nativeAssets.MarkPartial(*skinBin, "BIN header: " + error);
				return;
			}

			Assets::SkinAssetReferences references;
			if (!Assets::ResolveSkinAssets(skinDocument, "Jinx", 0, m_nativeHashIndex, references, &error))
			{
				m_nativeAssets.MarkPartial(*skinBin, "Skin references: " + error);
				return;
			}

			const std::string parentId = skinBin->Id();
			auto readReference = [this, &parentId](const std::string& path)
			{
				if (const auto* entry = m_nativeAssets.GetEntry(path))
				{
					const auto& payload = m_nativeAssets.ReadPayload(*entry, parentId);
					if (path.ends_with(".skn"))
					{
						Assets::SkinDocument skin;
						std::string skinError;
						if (!skin.Load(payload, &skinError))
							m_nativeAssets.MarkPartial(*entry, "SKN decode: " + skinError, parentId);
						else
						{
							Skin renderSkin;
							if (!renderSkin.LoadPayload(payload, &skinError))
								m_nativeAssets.MarkPartial(*entry, "SKN render conversion: " + skinError, parentId);
						}
					}
					else if (path.ends_with(".skl"))
					{
						Assets::SkeletonDocument skeleton;
						std::string skeletonError;
						if (!skeleton.Load(payload, &skeletonError))
							m_nativeAssets.MarkPartial(*entry, "SKL decode: " + skeletonError, parentId);
						else if (!skeleton.hierarchyComplete)
							m_nativeAssets.MarkPartial(*entry, "SKL hierarchy contains unsupported parent values", parentId);
						else
						{
							Skeleton renderSkeleton;
							if (!renderSkeleton.LoadPayload(payload, &skeletonError))
								m_nativeAssets.MarkPartial(*entry, "SKL render conversion: " + skeletonError, parentId);
						}
					}
					else if (path.ends_with("/animations/skin0.bin"))
					{
						Assets::BinDocument graphDocument;
						std::string graphError;
						if (!graphDocument.Load(payload, &graphError))
						{
							m_nativeAssets.MarkPartial(*entry, "Animation graph header: " + graphError, parentId);
						}
						else
						{
							Assets::AnimationGraphAssets graph;
							if (!Assets::ResolveAnimationGraphAssets(graphDocument, "Jinx", 0, m_nativeHashIndex, graph, &graphError))
								m_nativeAssets.MarkPartial(*entry, "Animation graph values: " + graphError, parentId);
						}
					}
					else if (path.ends_with(".tex") || path.ends_with(".dds"))
					{
						Assets::TextureDocument texture;
						std::string textureError;
						if (!texture.Load(payload, &textureError))
							m_nativeAssets.MarkPartial(*entry, "Texture decode: " + textureError, parentId);
					}
					return;
				}
				Assets::AssetEntry unresolved;
				unresolved.path.path = path;
				m_nativeAssets.MarkPartial(unresolved, "Referenced file is not in mounted archives", parentId);
			};
			readReference(references.skinMeshPath);
			readReference(references.skeletonPath);
			readReference(references.texturePath);
			if (references.animationGraphHash)
			{
				if (const std::string* graphPath = m_nativeHashIndex.Lookup(*references.animationGraphHash))
					readReference(*graphPath);
			}
		}
		catch (const std::exception&)
		{
			// Keep the native failure record available to the Assets panel.
		}
	}

	void LeagueModelApp::OnEvent()
	{
		for (const AppEvent& event : GetEvents())
		{
			switch (event.type)
			{
			case AppEvent::Type::KeyPressed:
				if (WantsKeyboardCapture())
					break;

				if (event.key == GLFW_KEY_ESCAPE)
					RequestClose();
				else if (event.key == GLFW_KEY_PAGE_UP)
					StepAnimation(-1);
				else if (event.key == GLFW_KEY_PAGE_DOWN)
					StepAnimation(1);
				break;

			case AppEvent::Type::MouseButtonPressed:
				if (event.mouseButton == GLFW_MOUSE_BUTTON_LEFT)
					m_leftMouseDown = !WantsMouseCapture();
				break;

			case AppEvent::Type::MouseButtonReleased:
				if (event.mouseButton == GLFW_MOUSE_BUTTON_LEFT)
					m_leftMouseDown = false;
				break;

			case AppEvent::Type::MouseMoved:
				if (m_hasMousePosition && m_leftMouseDown && !WantsMouseCapture())
					m_camera.OnRotate(static_cast<float>(event.x - m_lastMouseX), static_cast<float>(event.y - m_lastMouseY));

				m_lastMouseX = event.x;
				m_lastMouseY = event.y;
				m_hasMousePosition = true;
				break;

			case AppEvent::Type::MouseScrolled:
				if (!WantsMouseCapture())
					m_camera.OnZoom(static_cast<float>(event.scrollY));
				break;

			case AppEvent::Type::FramebufferResized:
				glViewport(0, 0, event.width, event.height);
				break;

			case AppEvent::Type::WindowCloseRequested:
				RequestClose();
				break;

			default:
				break;
			}
		}
	}

	void LeagueModelApp::OnUpdate(float deltaTime)
	{
		Spek::File::Update();
		if (!m_initialCharacterLoadRequested && m_wadFileSystem != nullptr && m_wadFileSystem->IsIndexed())
		{
			m_initialCharacterLoadRequested = true;
			m_character.Load("Jinx", 0);
		}
		RefreshAnimationList();

		m_character.currentTime += deltaTime;
		if (!m_nativeAnimationInjected && m_nativeAnimationPreview != nullptr && (m_character.loadState & CharacterLoadState::Loaded) == CharacterLoadState::Loaded)
		{
			m_character.animations.emplace(m_nativeAnimationPreview->name, m_nativeAnimationPreview);
			m_nativeAnimationInjected = true;
		}
		m_character.Update(m_pose);
		m_renderer.EnsureUploaded(m_character);
		if (m_argc > 2 && std::strcmp(m_argv[2], "--smoke") == 0)
		{
			static int frames = 0;
			static int champion = 0;
			static const char* names[] = { "Jinx", "Ahri", "Kassadin", "Jinx" };
			if ((m_character.loadState & CharacterLoadState::FailedBitSet) != 0)
				throw std::runtime_error("SMOKE load failure: " + m_character.loadError);
			if (m_renderer.IsReady() && (m_character.loadState & CharacterLoadState::Loaded) == CharacterLoadState::Loaded && ++frames >= 10)
			{
				printf("VIEWER_CHECK %s vertices=%zu bones=%zu animations=%zu gl_error=%u\n", names[champion], m_character.skin.vertices.size(), m_character.skeleton.bones.size(), m_character.animations.size(), glGetError());
				fflush(stdout);
				frames = 0;
				if (++champion == 4) RequestClose();
				else m_character.Load(names[champion], 0);
			}
		}
		UpdateWindowTitle();
	}

	void LeagueModelApp::OnRender()
	{
		int framebufferWidth = 0;
		int framebufferHeight = 0;
		glfwGetFramebufferSize(GetWindow(), &framebufferWidth, &framebufferHeight);
		glViewport(0, 0, framebufferWidth, framebufferHeight);

		glClearColor(0.15f, 0.18f, 0.24f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (!m_renderer.IsReady())
			return;

		const float aspectRatio = framebufferHeight > 0 ? static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight) : 1.0f;
		const glm::mat4 modelMatrix = glm::translate(m_character.center);
		m_renderer.Draw(
			m_character,
			m_pose,
			modelMatrix,
			m_camera.GetViewMatrix(),
			m_camera.GetProjectionMatrix(aspectRatio));
	}

	void LeagueModelApp::OnGuiRender()
	{
		RenderUI(m_character, &m_nativeAssets);
	}

	void LeagueModelApp::OnShutdown()
	{
		m_character.Reset();
		m_renderer.Shutdown();
	}

	void LeagueModelApp::RefreshAnimationList()
	{
		std::vector<std::string> animationNames;
		animationNames.reserve(m_character.animations.size());
		for (const auto& animation : m_character.animations)
			animationNames.push_back(animation.first);

		std::sort(animationNames.begin(), animationNames.end());
		if (animationNames == m_animationNames)
			return;

		const int previousIndex = FindCurrentAnimationIndex();
		m_animationNames = std::move(animationNames);
		m_currentAnimationIndex = previousIndex;

		if (m_currentAnimationIndex < 0 && !m_animationNames.empty())
		{
			m_currentAnimationIndex = FindDefaultAnimationIndex();
			if (m_currentAnimationIndex >= 0)
				m_character.PlayAnimation(m_animationNames[static_cast<size_t>(m_currentAnimationIndex)].c_str());
		}
	}

	int LeagueModelApp::FindDefaultAnimationIndex() const
	{
		if (m_animationNames.empty())
			return -1;

		auto containsIgnoreCase = [](const std::string& value, const char* needle)
		{
			std::string lowered = value;
			std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
			return lowered.find(needle) != std::string::npos;
		};

		for (size_t index = 0; index < m_animationNames.size(); ++index)
			if (containsIgnoreCase(m_animationNames[index], "idle"))
				return static_cast<int>(index);

		for (size_t index = 0; index < m_animationNames.size(); ++index)
			if (containsIgnoreCase(m_animationNames[index], "loop"))
				return static_cast<int>(index);

		return 0;
	}

	int LeagueModelApp::FindCurrentAnimationIndex() const
	{
		if (m_character.currentAnimation == nullptr)
			return -1;

		for (size_t index = 0; index < m_animationNames.size(); ++index)
		{
			const auto animationIt = m_character.animations.find(m_animationNames[index]);
			if (animationIt != m_character.animations.end() && animationIt->second.get() == m_character.currentAnimation)
				return static_cast<int>(index);
		}

		return -1;
	}

	void LeagueModelApp::StepAnimation(int direction)
	{
		if (m_animationNames.empty())
			return;

		if (m_currentAnimationIndex < 0)
			m_currentAnimationIndex = FindDefaultAnimationIndex();
		else
			m_currentAnimationIndex = (m_currentAnimationIndex + direction + static_cast<int>(m_animationNames.size())) % static_cast<int>(m_animationNames.size());

		m_character.PlayAnimation(m_animationNames[static_cast<size_t>(m_currentAnimationIndex)].c_str());
		UpdateWindowTitle();
	}

	void LeagueModelApp::UpdateWindowTitle()
	{
		const std::string characterName = m_character.modelName.empty() ? "Jinx" : m_character.modelName;
		std::string title = "LeagueModel - " + characterName;

		if ((m_character.loadState & CharacterLoadState::FailedBitSet) != 0)
		{
			title += " - Load Failed";
			SetWindowTitle(title);
			return;
		}

		if ((m_character.loadState & CharacterLoadState::Loaded) != CharacterLoadState::Loaded)
		{
			title += " - Loading";
			SetWindowTitle(title);
			return;
		}

		if (m_character.currentAnimation != nullptr && !m_character.currentAnimation->name.empty())
			title += " - " + m_character.currentAnimation->name;
		else
			title += " - No Animation";

		SetWindowTitle(title);
	}
}
