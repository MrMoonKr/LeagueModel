#include "app/league_model_app.hpp"

#include "league_lib/wad/wad_filesystem.hpp"
#include "ui.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

namespace LeagueModel
{
	namespace fs = std::filesystem;

	namespace
	{
		void MountDir(const char* wadPath)
		{
			if (wadPath == nullptr || !fs::exists(wadPath))
				return;

			const fs::path rootPath = wadPath;
			if (fs::is_regular_file(rootPath) && rootPath.filename() == "DATA.wad.client")
			{
				Spek::File::Mount<LeagueLib::WADFileSystem>(rootPath.parent_path().string().c_str());
				return;
			}

			const fs::path directWadPath = rootPath / "DATA.wad.client";
			if (fs::exists(directWadPath) && fs::is_regular_file(directWadPath))
			{
				Spek::File::Mount<LeagueLib::WADFileSystem>(rootPath.string().c_str());
				return;
			}

			for (auto& path : fs::recursive_directory_iterator(rootPath))
			{
				if (!path.is_regular_file())
					continue;

				if (path.path().filename() == "DATA.wad.client")
				{
					Spek::File::Mount<LeagueLib::WADFileSystem>(path.path().parent_path().string().c_str());
					return;
				}
			}
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
		m_camera.SetDistance(400.0f);
		m_character.Load("Jinx", 0);
		return true;
	}

	void LeagueModelApp::MountConfiguredRoots() const
	{
		if (!m_gameRootPath.empty() && fs::exists(m_gameRootPath))
			MountDir(m_gameRootPath.c_str());

		const fs::path localDirectory = fs::current_path();
		if (fs::exists(localDirectory))
			MountDir(localDirectory.string().c_str());
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
		RefreshAnimationList();

		m_character.currentTime += deltaTime;
		m_character.Update(m_pose);
		m_renderer.EnsureUploaded(m_character);
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
		RenderUI(m_character);
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
		std::string title = "LeagueModel - Jinx";

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
