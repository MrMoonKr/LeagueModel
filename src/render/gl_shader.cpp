#include "render/gl_shader.hpp"

#include <vector>
#include <cstdio>

namespace LeagueModel
{
	GLShader::~GLShader()
	{
		Destroy();
	}

	bool GLShader::CompileStage(GLuint shader, const char* source, const char* stageName)
	{
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);

		GLint succeeded = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &succeeded);
		if (succeeded == GL_TRUE)
			return true;

		GLint logLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		std::vector<char> logBuffer(static_cast<size_t>(logLength > 0 ? logLength : 1), '\0');
		glGetShaderInfoLog(shader, logLength, nullptr, logBuffer.data());
		printf("%s shader compilation failed:\n%s\n", stageName, logBuffer.data());
		return false;
	}

	bool GLShader::Create(const char* vertexSource, const char* fragmentSource)
	{
		Destroy();

		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		if (!CompileStage(vertexShader, vertexSource, "Vertex") || !CompileStage(fragmentShader, fragmentSource, "Fragment"))
		{
			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);
			return false;
		}

		m_program = glCreateProgram();
		glAttachShader(m_program, vertexShader);
		glAttachShader(m_program, fragmentShader);
		glLinkProgram(m_program);

		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		GLint linked = GL_FALSE;
		glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
		if (linked == GL_TRUE)
			return true;

		GLint logLength = 0;
		glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &logLength);
		std::vector<char> logBuffer(static_cast<size_t>(logLength > 0 ? logLength : 1), '\0');
		glGetProgramInfoLog(m_program, logLength, nullptr, logBuffer.data());
		printf("Shader link failed:\n%s\n", logBuffer.data());

		Destroy();
		return false;
	}

	void GLShader::Destroy()
	{
		if (m_program != 0)
		{
			glDeleteProgram(m_program);
			m_program = 0;
		}
	}

	void GLShader::Use() const
	{
		glUseProgram(m_program);
	}

	GLint GLShader::GetUniformLocation(const char* name) const
	{
		return glGetUniformLocation(m_program, name);
	}
}
