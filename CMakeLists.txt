cmake_minimum_required(VERSION 3.15)
project(LeagueModel LANGUAGES C CXX)

include(FetchContent)

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

ADD_SRC(LEAMOD_SRC	"App"						"src/app/gl_app.hpp"						"src/app/gl_app.cpp")
ADD_SRC(LEAMOD_SRC	"App"						"src/app/league_model_app.hpp"			"src/app/league_model_app.cpp")

ADD_SRC(LEAMOD_SRC	"Animation"					"src/animation.hpp"						"src/animation.cpp")
ADD_SRC(LEAMOD_SRC	"Animation"					"src/animation_graph.hpp"				"src/animation_graph.cpp")
ADD_SRC(LEAMOD_SRC	"Animation"					"src/animation_flags.hpp"				"")
ADD_SRC(LEAMOD_SRC	"Character"					"src/character.hpp"						"src/character.cpp")
ADD_SRC(LEAMOD_SRC	"Character\\Animation"		"src/character_animation.hpp"			"src/character_animation.cpp")
ADD_SRC(LEAMOD_SRC	"Image"						"src/managed_image.hpp"					"src/managed_image.cpp")
ADD_SRC(LEAMOD_SRC	"Render"					"src/render/character_pose.hpp"			"")
ADD_SRC(LEAMOD_SRC	"Render"					"src/render/orbit_camera.hpp"			"src/render/orbit_camera.cpp")
ADD_SRC(LEAMOD_SRC	"Render"					"src/render/gl_shader.hpp"				"src/render/gl_shader.cpp")
ADD_SRC(LEAMOD_SRC	"Render"					"src/render/character_renderer.hpp"		"src/render/character_renderer.cpp")
ADD_SRC(LEAMOD_SRC	"Skeleton"					"src/skeleton.hpp"						"src/skeleton.cpp")
ADD_SRC(LEAMOD_SRC	"Skin"						"src/skin.hpp"							"src/skin.cpp")

add_executable(LeagueModel ${LEAMOD_SRC})
target_include_directories(LeagueModel PRIVATE "ext/leaguelib/ext")
target_include_directories(LeagueModel PRIVATE "ext/leaguelib/ext/glm")
target_include_directories(LeagueModel PRIVATE "ext")
target_include_directories(LeagueModel PRIVATE "src")

if (WIN32)
	target_compile_definitions(LeagueModel PUBLIC WIN32_LEAN_AND_MEAN)
	target_compile_definitions(LeagueModel PUBLIC NOMINMAX)
endif ()

target_link_libraries(LeagueModel PRIVATE glad glfw OpenGL::GL LeagueLib)
set_target_properties(LeagueModel PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	CXX_STANDARD 20
	CXX_EXTENSIONS OFF
)
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT LeagueModel)
