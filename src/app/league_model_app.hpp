#pragma once

#include "app/gl_app.hpp"
#include "character.hpp"
#include "render/character_pose.hpp"
#include "render/character_renderer.hpp"
#include "render/orbit_camera.hpp"
#include "assets/asset_system.hpp"

#include <string>
#include <memory>
#include <vector>

namespace LeagueLib
{
	class WADFileSystem;
}

namespace LeagueModel
{
	class LeagueModelApp : public GLApp
	{
	public:
		LeagueModelApp(int argc, char** argv);

	protected:
		WindowConfig GetWindowConfig() const override;
		bool OnInit() override;
		void OnEvent() override;
		void OnUpdate(float deltaTime) override;
		void OnRender() override;
		void OnGuiRender() override;
		void OnShutdown() override;

	private:
		void MountConfiguredRoots();
		void InitializeNativeAssetPreview();
		void RefreshAnimationList();
		void StepAnimation(int direction);
		void UpdateWindowTitle();
		int FindDefaultAnimationIndex() const;
		int FindCurrentAnimationIndex() const;

		int m_argc = 0;
		char** m_argv = nullptr;
		bool m_leftMouseDown = false;
		double m_lastMouseX = 0.0;
		double m_lastMouseY = 0.0;
		bool m_hasMousePosition = false;
		bool m_initialCharacterLoadRequested = false;

		Character m_character;
		CharacterPose m_pose;
		CharacterRenderer m_renderer;
		OrbitCamera m_camera;
		std::vector<std::string> m_animationNames;
		int m_currentAnimationIndex = -1;
		std::string m_gameRootPath;
		LeagueLib::WADFileSystem* m_wadFileSystem = nullptr;
		Assets::GameHashIndex m_nativeHashIndex;
		Assets::AssetSystem m_nativeAssets;
		std::shared_ptr<Animation> m_nativeAnimationPreview;
		bool m_nativeAnimationInjected = false;
	};
}
