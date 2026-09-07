cmake_minimum_required(VERSION 3.15)
project(LeagueModel LANGUAGES C CXX)

include(FetchContent)
include(CTest)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
function(ADD_SRC var folder inc src)
	source_group(${folder} FILES ${inc} ${src})
	set(${var} ${${var}} ${inc} ${src} CACHE INTERNAL "")
endfunction(ADD_SRC)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.4
)

FetchContent_Declare(
	glad
	URL https://github.com/Dav1dde/glad/archive/refs/tags/v0.1.36.zip
)

FetchContent_MakeAvailable(glfw glad)

find_package(OpenGL REQUIRED)

# LeagueLib
add_subdirectory("ext/leaguelib")
set_target_properties(LeagueLib PROPERTIES FOLDER Libraries)

set(LEAMOD_SRC "" CACHE INTERNAL "LeagueModel Source Files")
ADD_SRC(LEAMOD_SRC	"\\"						""										"src/main.cpp")

ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/asset_types.hpp"					"src/assets/asset_types.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/asset_load_state.hpp"				"")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/load_record.hpp"					"")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/animation_graph_resolver.hpp"			"src/assets/animation_graph_resolver.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/anm_document.hpp"					"src/assets/anm_document.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/bin_document.hpp"					"src/assets/bin_document.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/character_asset_resolver.hpp"			"src/assets/character_asset_resolver.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/asset_archive.hpp"					"")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/asset_archive_set.hpp"				"src/assets/asset_archive_set.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/asset_registry.hpp"					"src/assets/asset_registry.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/asset_system.hpp"					"src/assets/asset_system.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/binary_reader.hpp"					"src/assets/binary_reader.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/directory_archive.hpp"				"src/assets/directory_archive.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/game_hash_index.hpp"					"src/assets/game_hash_index.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/skin_document.hpp"					"src/assets/skin_document.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/skeleton_document.hpp"					"src/assets/skeleton_document.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/texture_document.hpp"					"src/assets/texture_document.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/static_character_loader.hpp"					"src/assets/static_character_loader.cpp")
ADD_SRC(LEAMOD_SRC	"Assets"					"src/assets/wad_archive.hpp"					"src/assets/wad_archive.cpp")

ADD_SRC(LEAMOD_SRC	"App"						"src/app/gl_app.hpp"						"src/app/gl_app.cpp")
ADD_SRC(LEAMOD_SRC	"App"						"src/app/league_model_app.hpp"			"src/app/league_model_app.cpp")

ADD_SRC(LEAMOD_SRC	"Animation"					"src/animation.hpp"						"src/animation.cpp")
ADD_SRC(LEAMOD_SRC	"Animation"					"src/animation_graph.hpp"				"src/animation_graph.cpp")
ADD_SRC(LEAMOD_SRC	"Animation"					"src/animation_flags.hpp"				"")
ADD_SRC(LEAMOD_SRC	"Character"					"src/character.hpp"						"src/character.cpp")
ADD_SRC(LEAMOD_SRC	"\\"						"src/game_hashes.hpp"					"src/game_hashes.cpp")
ADD_SRC(LEAMOD_SRC	"Character\\Animation"		"src/character_animation.hpp"			"src/character_animation.cpp")
ADD_SRC(LEAMOD_SRC	"Image"						"src/managed_image.hpp"					"src/managed_image.cpp")
ADD_SRC(LEAMOD_SRC	"Render"					"src/render/character_pose.hpp"			"")
ADD_SRC(LEAMOD_SRC	"Render"					"src/render/orbit_camera.hpp"			"src/render/orbit_camera.cpp")
ADD_SRC(LEAMOD_SRC	"Render"					"src/render/gl_shader.hpp"				"src/render/gl_shader.cpp")
ADD_SRC(LEAMOD_SRC	"Render"					"src/render/character_renderer.hpp"		"src/render/character_renderer.cpp")
ADD_SRC(LEAMOD_SRC	"Skeleton"					"src/skeleton.hpp"						"src/skeleton.cpp")
ADD_SRC(LEAMOD_SRC	"Skin"						"src/skin.hpp"							"src/skin.cpp")
ADD_SRC(LEAMOD_SRC	"UI"						"src/ui.hpp"							"src/ui.cpp")

set(LEAMOD_IMGUI_SRC
	ext/sokol/ext/imgui/imgui.cpp
	ext/sokol/ext/imgui/imgui_draw.cpp
	ext/sokol/ext/imgui/imgui_tables.cpp
	ext/sokol/ext/imgui/imgui_widgets.cpp
	ext/sokol/ext/imgui/backends/imgui_impl_glfw.cpp
	ext/sokol/ext/imgui/backends/imgui_impl_opengl3.cpp
)
source_group("ThirdParty\\ImGui" FILES ${LEAMOD_IMGUI_SRC})

add_executable(LeagueModel ${LEAMOD_SRC} ${LEAMOD_IMGUI_SRC})
target_include_directories(LeagueModel PRIVATE "ext/leaguelib/ext")
target_include_directories(LeagueModel PRIVATE "ext/leaguelib/ext/glm")
target_include_directories(LeagueModel PRIVATE "ext")
target_include_directories(LeagueModel PRIVATE "ext/sokol/ext/imgui")
target_include_directories(LeagueModel PRIVATE "ext/sokol/ext/imgui/backends")
target_include_directories(LeagueModel PRIVATE "src")
target_include_directories(LeagueModel PRIVATE "ext/leaguelib/ext/zstd/lib")

if (WIN32)
	target_compile_definitions(LeagueModel PUBLIC WIN32_LEAN_AND_MEAN)
	target_compile_definitions(LeagueModel PUBLIC NOMINMAX)
endif ()
target_compile_definitions(LeagueModel PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

target_link_libraries(LeagueModel PRIVATE glad glfw OpenGL::GL LeagueLib libzstd_static)
set_target_properties(LeagueModel PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	CXX_STANDARD 20
	CXX_EXTENSIONS OFF
)
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT LeagueModel)

if (BUILD_TESTING)
	add_executable(LeagueModelAssetTests
		tests/assets_core_tests.cpp
		src/assets/asset_types.cpp
		src/assets/animation_graph_resolver.cpp
		src/assets/anm_document.cpp
		src/assets/bin_document.cpp
		src/assets/character_asset_resolver.cpp
		src/assets/asset_archive_set.cpp
		src/assets/asset_registry.cpp
		src/assets/asset_system.cpp
		src/assets/binary_reader.cpp
		src/assets/directory_archive.cpp
		src/assets/game_hash_index.cpp
		src/assets/skin_document.cpp
		src/assets/skeleton_document.cpp
		src/assets/texture_document.cpp
		src/assets/static_character_loader.cpp
		src/assets/wad_archive.cpp
	)
	target_include_directories(LeagueModelAssetTests PRIVATE "src" "ext/leaguelib/ext/zstd/lib")
	target_link_libraries(LeagueModelAssetTests PRIVATE libzstd_static)
	target_compile_features(LeagueModelAssetTests PRIVATE cxx_std_20)
	add_test(NAME LeagueModelAssetTests COMMAND LeagueModelAssetTests)
endif ()
