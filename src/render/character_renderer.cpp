#include "render/character_renderer.hpp"

#include "managed_image.hpp"

#include <cstddef>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/geometric.hpp>

namespace LeagueModel
{
	namespace
	{
		constexpr const char* g_vertexShaderSource = R"GLSL(
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aBoneIndices;
layout(location = 4) in vec4 aBoneWeights;

uniform mat4 uModel;
uniform mat4 uViewProjection;
layout(std140) uniform BoneMatrices
{
    mat4 uBones[255];
};

out vec2 vUV;

void main()
{
    mat4 skin =
        uBones[int(aBoneIndices.x)] * aBoneWeights.x +
        uBones[int(aBoneIndices.y)] * aBoneWeights.y +
        uBones[int(aBoneIndices.z)] * aBoneWeights.z +
        uBones[int(aBoneIndices.w)] * aBoneWeights.w;

    vec4 skinnedPosition = skin * vec4(aPosition, 1.0);
    vec4 worldPosition = uModel * skinnedPosition;
    vUV = aUV;
    gl_Position = uViewProjection * worldPosition;
}
)GLSL";

		constexpr const char* g_fragmentShaderSource = R"GLSL(
#version 330 core

in vec2 vUV;

uniform sampler2D uDiffuseTexture;

out vec4 fragColor;

void main()
{
    fragColor = texture(uDiffuseTexture, vUV);
}
)GLSL";
	}

	bool CharacterRenderer::Initialize()
	{
		if (!m_shader.Create(g_vertexShaderSource, g_fragmentShaderSource))
			return false;

		glGenBuffers(1, &m_vertexBuffer);
		glGenBuffers(1, &m_boneBuffer);
		m_fallbackTexture = CreateFallbackTexture();
		if (m_vertexBuffer == 0 || m_boneBuffer == 0 || m_fallbackTexture == 0)
			return false;

		const GLuint blockIndex = glGetUniformBlockIndex(m_shader.GetProgram(), "BoneMatrices");
		if (blockIndex == GL_INVALID_INDEX)
			return false;

		glUniformBlockBinding(m_shader.GetProgram(), blockIndex, 0);
		glBindBuffer(GL_UNIFORM_BUFFER, m_boneBuffer);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(CharacterPose::bones), nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_boneBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		return true;
	}

	void CharacterRenderer::Shutdown()
	{
		DestroySubmeshes();

		if (m_vertexBuffer != 0)
		{
			glDeleteBuffers(1, &m_vertexBuffer);
			m_vertexBuffer = 0;
		}

		if (m_boneBuffer != 0)
		{
			glDeleteBuffers(1, &m_boneBuffer);
			m_boneBuffer = 0;
		}

		if (m_fallbackTexture != 0)
		{
			glDeleteTextures(1, &m_fallbackTexture);
			m_fallbackTexture = 0;
		}

		m_shader.Destroy();
		m_uploadedSkinHash = 0;
	}

	void CharacterRenderer::DestroySubmeshes()
	{
		for (GLSubmesh& submesh : m_submeshes)
		{
			if (submesh.ebo != 0)
				glDeleteBuffers(1, &submesh.ebo);
			if (submesh.vao != 0)
				glDeleteVertexArrays(1, &submesh.vao);
		}
		m_submeshes.clear();
	}

	void CharacterRenderer::EnsureUploaded(const Character& character)
	{
		if ((character.loadState & CharacterLoadState::Loaded) != CharacterLoadState::Loaded)
			return;

		if (character.loadedSkinBinHash == 0 || character.loadedSkinBinHash == m_uploadedSkinHash)
			return;

		UploadCharacter(character);
	}

	GLuint CharacterRenderer::ResolveTextureId(const Character& character, u32 meshHash) const
	{
		const auto textureIt = character.textures.find(meshHash);
		if (textureIt != character.textures.end() && textureIt->second && textureIt->second->textureId != 0)
			return textureIt->second->textureId;

		if (character.globalTexture && character.globalTexture->textureId != 0)
			return character.globalTexture->textureId;

		return m_fallbackTexture;
	}

	void CharacterRenderer::UploadCharacter(const Character& character)
	{
		DestroySubmeshes();

		glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
		glBufferData(
			GL_ARRAY_BUFFER,
			static_cast<GLsizeiptr>(character.skin.vertices.size() * sizeof(Skin::Vertex)),
			character.skin.vertices.data(),
			GL_STATIC_DRAW);

		for (const Skin::Mesh& sourceMesh : character.skin.meshes)
		{
			GLSubmesh& submesh = m_submeshes.emplace_back();
			submesh.hash = sourceMesh.hash;
			submesh.indexCount = static_cast<u32>(sourceMesh.indexCount);
			submesh.textureId = ResolveTextureId(character, sourceMesh.hash);

			glGenVertexArrays(1, &submesh.vao);
			glGenBuffers(1, &submesh.ebo);

			glBindVertexArray(submesh.vao);
			glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ebo);
			glBufferData(
				GL_ELEMENT_ARRAY_BUFFER,
				static_cast<GLsizeiptr>(sourceMesh.indexCount * sizeof(u16)),
				sourceMesh.indices,
				GL_STATIC_DRAW);

			const GLsizei stride = static_cast<GLsizei>(sizeof(Skin::Vertex));
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Skin::Vertex, position)));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Skin::Vertex, uv)));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Skin::Vertex, normal)));
			glEnableVertexAttribArray(3);
			glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Skin::Vertex, boneIndices)));
			glEnableVertexAttribArray(4);
			glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Skin::Vertex, weights)));
		}

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		m_uploadedSkinHash = character.loadedSkinBinHash;
	}

	void CharacterRenderer::Draw(const Character& character, const CharacterPose& pose, const glm::mat4& modelMatrix, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
		if (!IsReady())
			return;

		m_shader.Use();

		const glm::mat4 viewProjection = projectionMatrix * viewMatrix;

		glUniformMatrix4fv(m_shader.GetUniformLocation("uModel"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
		glUniformMatrix4fv(m_shader.GetUniformLocation("uViewProjection"), 1, GL_FALSE, glm::value_ptr(viewProjection));
		glUniform1i(m_shader.GetUniformLocation("uDiffuseTexture"), 0);

		glBindBuffer(GL_UNIFORM_BUFFER, m_boneBuffer);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CharacterPose::bones), pose.bones.data());
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_boneBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		glActiveTexture(GL_TEXTURE0);
		for (size_t index = 0; index < m_submeshes.size() && index < character.meshes.size(); ++index)
		{
			if (!character.meshes[index].shouldRender)
				continue;

			const GLSubmesh& submesh = m_submeshes[index];
			glBindTexture(GL_TEXTURE_2D, submesh.textureId);
			glBindVertexArray(submesh.vao);
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(submesh.indexCount), GL_UNSIGNED_SHORT, nullptr);
		}

		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	GLuint CharacterRenderer::CreateFallbackTexture()
	{
		GLuint textureId = 0;
		const unsigned char whitePixel[4] = { 255, 255, 255, 255 };

		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
		glBindTexture(GL_TEXTURE_2D, 0);
		return textureId;
	}
}
