#pragma once

#include <spek/file/file.hpp>
#include <glad/glad.h>
#include <functional>

namespace LeagueModel
{
	struct ManagedImage
	{
		using OnLoadFunction = std::function<void(ManagedImage& skin)>;
		ManagedImage(const char* path, OnLoadFunction onImageLoaded = nullptr);
		~ManagedImage();

		GLuint textureId = 0;
		Spek::File::LoadState loadState = Spek::File::LoadState::NotLoaded;

	private:
		Spek::File::Handle file = nullptr;
	};
}
	
