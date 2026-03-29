# LeagueModel OpenGL Refactor Design

## Goals

- Replace the current `sokol_app + sokol_gfx + simgui` runtime with a `GLFW3 + OpenGL` application framework.
- Keep the asset pipeline intact:
  - `WAD` mounting
  - `BIN`, `SKN`, `SKL`, `ANM` parsing
  - animation graph loading
- Focus the renderer on the single required feature set:
  - textured character rendering
  - skeletal animation playback
  - three-point lighting for model viewing
  - orbit-style model viewer camera
- Use `Jinx` as the default validation character before ImGui is reintroduced.
- Use `PageUp` and `PageDown` to cycle animations during the non-ImGui phase.

## Non-Goals

- No PBR or material system rewrite.
- No renderer abstraction across multiple graphics APIs.
- No immediate ImGui port in the first pass.
- No changes to the existing `LeagueLib` loading/parsing model unless required for compatibility.

## Runtime Architecture

### GLApp

`GLApp` owns platform setup and the main loop.

Responsibilities:

- initialize and shut down GLFW
- create the OpenGL 3.3 core context
- load OpenGL functions through `glad`
- collect window/input events into a per-frame queue
- calculate frame delta time
- drive the loop in this order:
  1. poll OS events
  2. `OnEvent()`
  3. `OnUpdate(deltaTime)`
  4. `OnRender()`
  5. swap buffers

Public lifecycle:

- `InitInstance()`
- `Run()`
- `ExitInstance()`

Virtual hooks:

- `OnInit()`
- `OnEvent()`
- `OnUpdate(float deltaTime)`
- `OnRender()`
- `OnShutdown()`

### LeagueModelApp

`LeagueModelApp` derives from `GLApp` and owns app/game state.

Responsibilities:

- resolve the game root from command line or `config.ini`
- mount the League asset roots
- load the default test character: `Jinx`
- manage animation switching with `PageUp` and `PageDown`
- update the orbit camera
- call `Spek::File::Update()`
- update character animation pose
- submit the character to the renderer

## Data Flow

### Asset Flow

1. `LeagueModelApp` mounts the game root with `WADFileSystem`.
2. `Character::Load()` requests:
   - skin BIN
   - skeleton
   - skin mesh
   - textures
   - animation graph BIN
   - animation clips
3. `Character` remains responsible for:
   - resolving file names from BIN data
   - applying skeleton remapping
   - tracking load state
   - maintaining animation selection and submesh visibility state
4. `CharacterRenderer` uploads GPU resources only after the character has completed CPU-side asset resolution.

### Animation Flow

1. `Character::Load()` loads the animation graph and animation clips.
2. `Character::Update()` computes the current pose palette on the CPU.
3. `CharacterRenderer` uploads the bone palette to the shader every frame.
4. Vertex skinning happens in the vertex shader.

## Rendering Architecture

### CharacterRenderer

`CharacterRenderer` owns the OpenGL draw resources for the active character.

Responsibilities:

- compile/link the skinned mesh shader
- upload shared vertex data
- upload per-submesh index buffers
- bind textures
- draw visible submeshes
- rebuild GPU state when the loaded skin changes

Renderer inputs:

- `Character`
- `CharacterPose`
- camera view/projection matrices
- model transform
- lighting rig settings

### CharacterPose

`CharacterPose` is the CPU-to-GPU animation payload.

Contents:

- `glm::mat4 bones[255]`

This replaces the old `AnimatedMeshParametersVS_t` dependency from the generated `sokol` shader header.

### OrbitCamera

`OrbitCamera` replaces the ad-hoc camera state in `main.cpp`.

Responsibilities:

- orbit around the model target
- zoom in/out
- optional pan support
- expose `GetViewMatrix()` and `GetProjectionMatrix(aspect)`

Input mapping:

- left mouse drag: orbit
- mouse wheel: zoom

### Lighting Model

The first OpenGL renderer uses a simple forward shader with:

- albedo texture sampling
- diffuse Lambert lighting
- light specular highlight
- ambient term
- three directional lights:
  - key
  - fill
  - rim

This is intentionally a model-viewer lighting setup, not a physically based scene lighting system.

## Character State Refactor

The existing `Character` class currently owns `sokol` GPU resources.

That ownership moves out of `Character`.

`Character` keeps:

- load state flags
- parsed skin/skeleton/animation data
- current animation
- texture references
- submesh visibility state

`Character` no longer owns:

- `sg_pipeline`
- `sg_bindings`
- `sg_image`

The `MeshGenCompleted` state remains as the signal that CPU-side preparation is complete. GPU upload becomes the renderer's responsibility.

## Input and Event Strategy

`GLApp` stores an event queue for the current frame.

`LeagueModelApp::OnEvent()` processes:

- key press/release
- mouse move
- mouse button
- scroll
- resize

Behavior during the non-ImGui phase:

- `PageUp`: previous animation
- `PageDown`: next animation
- `Escape`: request app exit

## Window Title

During the non-ImGui phase the window title should expose enough state for quick validation.

Recommended format:

- `LeagueModel - Jinx - <animation name>`

Fallbacks:

- `LeagueModel - Jinx - Loading`
- `LeagueModel - Jinx - No Animation`

## CMake Strategy

External modules should be brought in through `FetchContent`.

Planned external dependencies:

- `glfw`
- `glad`

Local dependencies retained:

- `LeagueLib`
- `stb_image.h`
- `dds_reader.hpp`
- `glm` from `LeagueLib`

The old `ext/sokol` target is removed from the active build graph.

## File Layout

Planned layout:

- `src/app/gl_app.hpp`
- `src/app/gl_app.cpp`
- `src/app/league_model_app.hpp`
- `src/app/league_model_app.cpp`
- `src/render/character_pose.hpp`
- `src/render/gl_shader.hpp`
- `src/render/gl_shader.cpp`
- `src/render/orbit_camera.hpp`
- `src/render/orbit_camera.cpp`
- `src/render/character_renderer.hpp`
- `src/render/character_renderer.cpp`

The old `src/main.cpp` becomes a thin application entry point.

## Migration Plan

### Phase 1

- add design docs
- add `GLApp`
- switch CMake to `GLFW + glad`
- create OpenGL entry point and empty window loop

### Phase 2

- refactor `ManagedImage` from `sg_image` to OpenGL texture objects
- remove `sg_*` usage from `Character`
- add `CharacterPose`

### Phase 3

- implement `OrbitCamera`
- implement `CharacterRenderer`
- render `Jinx` with default animation
- add `PageUp/PageDown` animation cycling

### Phase 4

- restore optional tooling overlays
- reintroduce ImGui on top of the new app framework

## Validation Checklist

- application launches through `GLApp`
- game root resolves from `config.ini`
- `Jinx` loads without manual selection UI
- textured mesh is visible
- animation plays
- `PageUp/PageDown` switches animations
- mouse orbit and wheel zoom work
- no runtime dependency on `sokol_app`, `sokol_gfx`, or `simgui`
