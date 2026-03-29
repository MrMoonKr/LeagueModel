#include "app/gl_app.hpp"

#include <cstdio>

namespace LeagueModel
{
	WindowConfig GLApp::GetWindowConfig() const
	{
		return {};
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
		glfwSetCursorPosCallback(m_window, &GLApp::CursorPositionCallback);
		glfwSetMouseButtonCallback(m_window, &GLApp::MouseButtonCallback);
		glfwSetScrollCallback(m_window, &GLApp::ScrollCallback);
		glfwSetFramebufferSizeCallback(m_window, &GLApp::FramebufferSizeCallback);
		glfwSetWindowCloseCallback(m_window, &GLApp::WindowCloseCallback);

		m_initialized = OnInit();
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
			OnRender();
			glfwSwapBuffers(m_window);
			m_events.clear();
		}

		return 0;
	}

	void GLApp::ExitInstance()
	{
		if (m_initialized)
			OnShutdown();

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

	void GLApp::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		auto* app = reinterpret_cast<GLApp*>(glfwGetWindowUserPointer(window));
		if (app == nullptr)
			return;

		AppEvent event = {};
		event.key = key;
		event.scancode = scancode;
		event.mods = mods;
		event.type = action == GLFW_PRESS ? AppEvent::Type::KeyPressed : AppEvent::Type::KeyReleased;
		if (action == GLFW_REPEAT)
			event.type = AppEvent::Type::KeyPressed;
		app->PushEvent(event);
	}

	void GLApp::CursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
	{
		auto* app = reinterpret_cast<GLApp*>(glfwGetWindowUserPointer(window));
		if (app == nullptr)
			return;

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
}
