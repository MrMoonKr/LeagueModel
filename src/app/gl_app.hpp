#pragma once

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>

#include <string>
#include <vector>

namespace LeagueModel
{
	struct AppEvent
	{
		enum class Type
		{
			KeyPressed,
			KeyReleased,
			MouseButtonPressed,
			MouseButtonReleased,
			MouseMoved,
			MouseScrolled,
			FramebufferResized,
			WindowCloseRequested
		};

		Type type = Type::MouseMoved;
		int key = 0;
		int scancode = 0;
		int mods = 0;
		int mouseButton = 0;
		double x = 0.0;
		double y = 0.0;
		double scrollX = 0.0;
		double scrollY = 0.0;
		int width = 0;
		int height = 0;
	};

	struct WindowConfig
	{
		int width = 1280;
		int height = 720;
		std::string title = "LeagueModel";
	};

	class GLApp
	{
	public:
		virtual ~GLApp() = default;

		bool InitInstance();
		int Run();
		void ExitInstance();

	protected:
		virtual WindowConfig GetWindowConfig() const;
		virtual bool OnInit() = 0;
		virtual void OnEvent() = 0;
		virtual void OnUpdate(float deltaTime) = 0;
		virtual void OnRender() = 0;
		virtual void OnGuiRender();
		virtual void OnShutdown() = 0;

		void RequestClose();
		void SetWindowTitle(const std::string& title);
		bool WantsMouseCapture() const;
		bool WantsKeyboardCapture() const;

		GLFWwindow* GetWindow() const { return m_window; }
		const std::vector<AppEvent>& GetEvents() const { return m_events; }

	private:
		bool InitGui();
		void ShutdownGui();
		void BeginGuiFrame();
		void EndGuiFrame();
		float QueryWindowScale() const;
		void UpdateGuiScale(float scale);

		static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void CharCallback(GLFWwindow* window, unsigned int codepoint);
		static void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
		static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
		static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
		static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
		static void WindowContentScaleCallback(GLFWwindow* window, float xscale, float yscale);
		static void WindowCloseCallback(GLFWwindow* window);

		void PushEvent(const AppEvent& event);

		GLFWwindow* m_window = nullptr;
		std::vector<AppEvent> m_events;
		ImGuiStyle m_baseGuiStyle = {};
		bool m_initialized = false;
		bool m_guiInitialized = false;
		float m_guiScale = 1.0f;
		bool m_exitRequested = false;
	};
}
