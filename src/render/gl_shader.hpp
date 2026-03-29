#pragma once

#include <glad/glad.h>

namespace LeagueModel
{
	class GLShader
	{
	public:
		~GLShader();

		bool Create(const char* vertexSource, const char* fragmentSource);
		void Destroy();
		void Use() const;
		GLint GetUniformLocation(const char* name) const;
		GLuint GetProgram() const { return m_program; }

	private:
		bool CompileStage(GLuint shader, const char* source, const char* stageName);

		GLuint m_program = 0;
	};
}
