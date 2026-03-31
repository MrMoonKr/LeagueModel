#include "app/gl_app.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace LeagueModel
{
	WindowConfig GLApp::GetWindowConfig() const
	{
		return {};
	}

	void GLApp::OnGuiRender()
	{
	}

	bool GLApp::InitInstance()
	{
		if (!glfwInit())
		{
			printf("GLFW initialization failed.\n");
			return false;
		}

		const WindowConfig config = GetWindowConfig();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
		glfwWindowHint(GLFW_SAMPLES, 4);
		glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

		m_window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
		if (m_window == nullptr)
		{
			printf("Unable to create GLFW window.\n");
			glfwTerminate();
			return false;
		}

		glfwMakeContextCurrent(m_window);
		glfwSwapInterval(1);

		if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
		{
			printf("gladLoadGLLoader failed.\n");
			glfwDestroyWindow(m_window);
			m_window = nullptr;
			glfwTerminate();
			return false;
		}

		glfwSetWindowUserPointer(m_window, this);
		glfwSetKeyCallback(m_window, &GLApp::KeyCallback);
		glfwSetCharCallback(m_window, &GLApp::CharCallback);
		glfwSetCursorPosCallback(m_window, &GLApp::CursorPositionCallback);
		glfwSetMouseButtonCallback(m_window, &GLApp::MouseButtonCallback);
		glfwSetScrollCallback(m_window, &GLApp::ScrollCallback);
		glfwSetFramebufferSizeCallback(m_window, &GLApp::FramebufferSizeCallback);
		glfwSetWindowContentScaleCallback(m_window, &GLApp::WindowContentScaleCallback);
		glfwSetWindowCloseCallback(m_window, &GLApp::WindowCloseCallback);

		if (!InitGui())
		{
			glfwDestroyWindow(m_window);
			m_window = nullptr;
			glfwTerminate();
			return false;
		}

		m_initialized = OnInit();
		if (!m_initialized)
		{
			ShutdownGui();
			glfwDestroyWindow(m_window);
			m_window = nullptr;
			glfwTerminate();
		}

		return m_initialized;
	}

	int GLApp::Run()
	{
		if (!m_initialized || m_window == nullptr)
			return -1;

		double lastTime = glfwGetTime();
		while (!m_exitRequested && !glfwWindowShouldClose(m_window))
		{
			glfwPollEvents();
			OnEvent();

			const double currentTime = glfwGetTime();
			const float deltaTime = static_cast<float>(currentTime - lastTime);
			lastTime = currentTime;

			OnUpdate(deltaTime);
			BeginGuiFrame();
			OnRender();
			OnGuiRender();
			EndGuiFrame();
			glfwSwapBuffers(m_window);
			m_events.clear();
		}

		return 0;
	}

	void GLApp::ExitInstance()
	{
		if (m_initialized)
			OnShutdown();

		ShutdownGui();

		if (m_window != nullptr)
		{
			glfwDestroyWindow(m_window);
			m_window = nullptr;
		}

		glfwTerminate();
		m_events.clear();
		m_initialized = false;
		m_exitRequested = false;
	}

	bool GLApp::WantsMouseCapture() const
	{
		return m_guiInitialized && ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
	}

	bool GLApp::WantsKeyboardCapture() const
	{
		return m_guiInitialized && ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
	}

	void GLApp::RequestClose()
	{
		m_exitRequested = true;
	}

	void GLApp::SetWindowTitle(const std::string& title)
	{
		if (m_window != nullptr)
			glfwSetWindowTitle(m_window, title.c_str());
	}

	void GLApp::PushEvent(const AppEvent& event)
	{
		m_events.push_back(event);
	}

	bool GLApp::InitGui()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		m_baseGuiStyle = ImGui::GetStyle();

		if (!ImGui_ImplGlfw_InitForOpenGL(m_window, false))
		{
			ImGui::DestroyContext();
			return false;
		}

		if (!ImGui_ImplOpenGL3_Init("#version 330"))
		{
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
			return false;
		}

		m_guiInitialized = true;
		UpdateGuiScale(QueryWindowScale());
		return true;
	}

	void GLApp::ShutdownGui()
	{
		if (!m_guiInitialized)
			return;

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		m_guiInitialized = false;
	}

	void GLApp::BeginGuiFrame()
	{
		if (!m_guiInitialized)
			return;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void GLApp::EndGuiFrame()
	{
		if (!m_guiInitialized)
			return;

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	float GLApp::QueryWindowScale() const
	{
		if (m_window == nullptr)
			return 1.0f;

		float xscale = 1.0f;
		float yscale = 1.0f;
		glfwGetWindowContentScale(m_window, &xscale, &yscale);
		return std::max(1.0f, std::max(xscale, yscale));
	}

	void GLApp::UpdateGuiScale(float scale)
	{
		if (!m_guiInitialized || ImGui::GetCurrentContext() == nullptr)
			return;

		const float resolvedScale = std::max(1.0f, scale);
		if (std::fabs(resolvedScale - m_guiScale) < 0.01f)
			return;

		m_guiScale = resolvedScale;

		ImGuiStyle& style = ImGui::GetStyle();
		style = m_baseGuiStyle;
		style.ScaleAllSizes(m_guiScale);

		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->Clear();

		ImFontConfig fontConfig;
		fontConfig.SizePixels = 13.0f * m_guiScale;
		io.Fonts->AddFontDefault(&fontConfig);
		io.FontGlobalScale = 1.0f;

		ImGui_ImplOpenGL3_DestroyFontsTexture();
		ImGui_ImplOpenGL3_CreateFontsTexture();
	}

	void GLApp::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		auto* app = reinterpret_cast<GLApp*>(glfwGetWindowUserPointer(window));
		if (app == nullptr)
			return;

		if (app->m_guiInitialized)
			ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);

		AppEvent event = {};
		event.key = key;
		event.scancode = scancode;
		event.mods = mods;
		event.type = action == GLFW_PRESS ? AppEvent::Type::KeyPressed : AppEvent::Type::KeyReleased;
		if (action == GLFW_REPEAT)
			event.type = AppEvent::Type::KeyPressed;
		app->PushEvent(event);
	}

	void GLApp::CharCallback(GLFWwindow* window, unsigned int codepoint)
	{
		auto* app = reinterpret_cast<GLApp*>(glfwGetWindowUserPointer(window));
		if (app == nullptr || !app->m_guiInitialized)
			return;

		ImGui_ImplGlfw_CharCallback(window, codepoint);
	}

	void GLApp::CursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
	{
		auto* app = reinterpret_cast<GLApp*>(glfwGetWindowUserPointer(window));
		if (app == nullptr)
			return;

		if (app->m_guiInitialized)
			ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);

		AppEvent event = {};
		event.type = AppEvent::Type::MouseMoved;
		event.x = xpos;
		event.y = ypos;
		app->PushEvent(event);
	}

	void GLApp::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
	{
		auto* app = reinterpret_cast<GLApp*>(glfwGetWindowUserPointer(window));
		if (app == nullptr)
			return;

		if (app->m_guiInitialized)
			ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

		AppEvent event = {};
		event.mouseButton = button;
		event.mods = mods;
		event.type = action == GLFW_PRESS ? AppEvent::Type::MouseButtonPressed : AppEvent::Type::MouseButtonReleased;
		app->PushEvent(event);
	}

	void GLApp::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
	{
		auto* app = reinterpret_cast<GLApp*>(glfwGetWindowUserPointer(window));
		if (app == nullptr)
			return;

		if (app->m_guiInitialized)
			ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

		AppEvent event = {};
		event.type = AppEvent::Type::MouseScrolled;
		event.scrollX = xoffset;
		event.scrollY = yoffset;
		app->PushEvent(event);
	}

	void GLApp::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
	{
		auto* app = reinterpret_cast<GLApp*>(glfwGetWindowUserPointer(window));
		if (app == nullptr)
			return;

		AppEvent event = {};
		event.type = AppEvent::Type::FramebufferResized;
		event.width = width;
		event.height = height;
		app->PushEvent(event);
	}

	void GLApp::WindowCloseCallback(GLFWwindow* window)
	{
		auto* app = reinterpret_cast<GLApp*>(glfwGetWindowUserPointer(window));
		if (app == nullptr)
			return;

		AppEvent event = {};
		event.type = AppEvent::Type::WindowCloseRequested;
		app->PushEvent(event);
	}

	void GLApp::WindowContentScaleCallback(GLFWwindow* window, float xscale, float yscale)
	{
		auto* app = reinterpret_cast<GLApp*>(glfwGetWindowUserPointer(window));
		if (app == nullptr)
			return;

		app->UpdateGuiScale(std::max(xscale, yscale));
	}
}
