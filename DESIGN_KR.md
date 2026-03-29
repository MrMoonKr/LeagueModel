# LeagueModel OpenGL 리팩토링 설계

## 목표

- 현재의 `sokol_app + sokol_gfx + simgui` 기반 런타임을 `GLFW3 + OpenGL` 구조로 교체합니다.
- 에셋 파이프라인은 유지합니다.
  - `WAD` 마운트
  - `BIN`, `SKN`, `SKL`, `ANM` 파싱
  - 애니메이션 그래프 로딩
- 렌더러는 현재 필요한 모델 뷰어 기능에만 집중합니다.
  - 텍스처 기반 캐릭터 렌더링
  - 스켈레탈 애니메이션 재생
  - 3점 조명
  - 오빗 카메라
- ImGui 재도입 전까지 기본 검증 캐릭터는 `Jinx`로 고정합니다.
- ImGui 없는 단계에서는 `PageUp`, `PageDown`으로 애니메이션을 전환합니다.

## 제외 범위

- PBR 또는 재질 시스템 전면 개편은 하지 않습니다.
- 멀티 그래픽 API 추상화는 하지 않습니다.
- 1차 단계에서 ImGui 이식은 하지 않습니다.
- `LeagueLib`의 파싱 구조는 호환성 유지에 필요한 수준만 수정합니다.

## 런타임 구조

### GLApp

`GLApp`은 플랫폼 초기화와 메인 루프를 담당합니다.

역할:

- GLFW 초기화/종료
- OpenGL 3.3 core 컨텍스트 생성
- `glad`를 통한 OpenGL 함수 로딩
- 프레임 단위 이벤트 큐 수집
- delta time 계산
- 다음 순서의 메인 루프 실행
  1. OS 이벤트 폴링
  2. `OnEvent()`
  3. `OnUpdate(deltaTime)`
  4. `OnRender()`
  5. 버퍼 스왑

공개 생명주기 함수:

- `InitInstance()`
- `Run()`
- `ExitInstance()`

가상 함수:

- `OnInit()`
- `OnEvent()`
- `OnUpdate(float deltaTime)`
- `OnRender()`
- `OnShutdown()`

### LeagueModelApp

`LeagueModelApp`은 `GLApp`을 상속받는 실제 애플리케이션 클래스입니다.

역할:

- 명령행 또는 `config.ini`에서 게임 루트 해석
- League 에셋 루트 마운트
- 기본 테스트 캐릭터 `Jinx` 로드
- `PageUp`, `PageDown` 애니메이션 전환
- 오빗 카메라 업데이트
- `Spek::File::Update()` 호출
- 캐릭터 애니메이션 포즈 업데이트
- 렌더러에 캐릭터 제출

## 데이터 흐름

### 에셋 로딩 흐름

1. `LeagueModelApp`이 게임 루트를 `WADFileSystem`으로 마운트합니다.
2. `Character::Load()`가 다음 리소스를 요청합니다.
   - skin BIN
   - skeleton
   - skin mesh
   - textures
   - animation graph BIN
   - animation clips
3. `Character`는 다음 책임을 유지합니다.
   - BIN 내부 문자열에서 실제 파일명 해석
   - skeleton remap 적용
   - load state 관리
   - 현재 애니메이션 및 서브메시 표시 상태 관리
4. `CharacterRenderer`는 CPU 측 준비가 끝난 뒤에만 GPU 리소스를 업로드합니다.

### 애니메이션 흐름

1. `Character::Load()`가 animation graph와 animation clip을 로드합니다.
2. `Character::Update()`가 CPU에서 현재 포즈 팔레트를 계산합니다.
3. `CharacterRenderer`가 본 행렬 배열을 셰이더에 업로드합니다.
4. 정점 스키닝은 vertex shader에서 수행합니다.

## 렌더링 구조

### CharacterRenderer

`CharacterRenderer`는 활성 캐릭터에 대한 OpenGL 드로우 리소스를 소유합니다.

역할:

- 스키닝 셰이더 compile/link
- 공유 vertex buffer 업로드
- 서브메시별 index buffer 업로드
- 텍스처 바인딩
- 표시 가능한 서브메시 드로우
- 스킨 변경 시 GPU 리소스 재생성

입력:

- `Character`
- `CharacterPose`
- 카메라의 view/projection 행렬
- model transform
- lighting rig 설정

### CharacterPose

`CharacterPose`는 CPU에서 GPU로 전달되는 애니메이션 포즈 데이터입니다.

구성:

- `glm::mat4 bones[255]`

기존 `sokol` 셰이더 헤더의 `AnimatedMeshParametersVS_t` 의존성을 이것으로 대체합니다.

### OrbitCamera

현재 `main.cpp`의 임시 카메라 상태를 `OrbitCamera`로 분리합니다.

역할:

- 모델 타깃 중심 회전
- 줌
- 필요 시 팬
- `GetViewMatrix()`, `GetProjectionMatrix(aspect)` 제공

입력 매핑:

- 마우스 왼쪽 드래그: 회전
- 마우스 휠: 줌

### 조명 모델

초기 OpenGL 렌더러는 단순 forward 셰이더를 사용합니다.

- 알베도 텍스처 샘플링
- Lambert diffuse
- 가벼운 specular
- ambient term
- directional light 3개
  - key
  - fill
  - rim

이는 모델뷰용 조명 세팅이며, PBR 기반 장면 조명을 목표로 하지 않습니다.

## Character 구조 변경

현재 `Character`는 `sokol` GPU 리소스를 직접 소유하고 있습니다.

이 책임은 렌더러로 이동합니다.

`Character`가 유지하는 것:

- load state flag
- 파싱된 skin/skeleton/animation 데이터
- 현재 애니메이션
- texture 참조
- 서브메시 표시 상태

`Character`가 더 이상 소유하지 않는 것:

- `sg_pipeline`
- `sg_bindings`
- `sg_image`

기존 `MeshGenCompleted`는 CPU 측 준비 완료 신호로 유지하고, 실제 GPU 업로드는 렌더러 책임으로 분리합니다.

## 입력 및 이벤트 전략

`GLApp`은 프레임 단위 이벤트 큐를 저장합니다.

`LeagueModelApp::OnEvent()`는 다음을 처리합니다.

- 키 입력
- 마우스 이동
- 마우스 버튼
- 스크롤
- 리사이즈

ImGui 없는 단계의 동작:

- `PageUp`: 이전 애니메이션
- `PageDown`: 다음 애니메이션
- `Escape`: 종료 요청

## 창 제목

ImGui가 없는 동안에는 창 제목이 상태 확인용 디버그 UI 역할을 일부 대신합니다.

권장 형식:

- `LeagueModel - Jinx - <animation name>`

대체 상태:

- `LeagueModel - Jinx - Loading`
- `LeagueModel - Jinx - No Animation`

## CMake 전략

외부 모듈은 `FetchContent`로 가져옵니다.

예정 외부 의존성:

- `glfw`
- `glad`

유지할 로컬 의존성:

- `LeagueLib`
- `stb_image.h`
- `dds_reader.hpp`
- `LeagueLib` 내부의 `glm`

기존 `ext/sokol` 타깃은 활성 빌드 그래프에서 제거합니다.

## 파일 배치

예정 구조:

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

기존 `src/main.cpp`는 얇은 엔트리 포인트로 축소합니다.

## 이행 단계

### 1단계

- 설계 문서 추가
- `GLApp` 추가
- CMake를 `GLFW + glad`로 전환
- OpenGL 빈 창과 루프 구성

### 2단계

- `ManagedImage`를 `sg_image`에서 OpenGL texture object 방식으로 전환
- `Character` 내부의 `sg_*` 제거
- `CharacterPose` 추가

### 3단계

- `OrbitCamera` 구현
- `CharacterRenderer` 구현
- `Jinx` 기본 렌더링
- `PageUp/PageDown` 애니메이션 전환

### 4단계

- 선택형 디버그 오버레이 복원
- 새로운 앱 구조 위에 ImGui 재도입

## 검증 체크리스트

- 애플리케이션이 `GLApp`을 통해 실행된다
- `config.ini`에서 게임 루트를 해석한다
- UI 없이 `Jinx`가 기본 로드된다
- 텍스처가 적용된 메시가 보인다
- 애니메이션이 재생된다
- `PageUp/PageDown`으로 애니메이션 전환이 된다
- 마우스 회전 및 휠 줌이 동작한다
- 런타임이 더 이상 `sokol_app`, `sokol_gfx`, `simgui`에 의존하지 않는다
