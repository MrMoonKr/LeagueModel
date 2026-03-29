#pragma once

#include "character.hpp"
#include "render/character_pose.hpp"
#include "render/gl_shader.hpp"

#include <glad/glad.h>

#include <glm/mat4x4.hpp>

#include <vector>

namespace LeagueModel
{
	class CharacterRenderer
	{
	public:
		bool Initialize();
		void Shutdown();

		void EnsureUploaded(const Character& character);
		void Draw(const Character& character, const CharacterPose& pose, const glm::mat4& modelMatrix, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

		bool IsReady() const { return m_uploadedSkinHash != 0 && !m_submeshes.empty(); }

	private:
		struct GLSubmesh
		{
			GLuint vao = 0;
			GLuint ebo = 0;
			GLuint textureId = 0;
			u32 hash = 0;
			u32 indexCount = 0;
		};

		void DestroySubmeshes();
		void UploadCharacter(const Character& character);
		GLuint ResolveTextureId(const Character& character, u32 meshHash) const;
		GLuint CreateFallbackTexture();

		GLShader m_shader;
		GLuint m_vertexBuffer = 0;
		GLuint m_boneBuffer = 0;
		GLuint m_fallbackTexture = 0;
		u32 m_uploadedSkinHash = 0;
		std::vector<GLSubmesh> m_submeshes;
	};
}
