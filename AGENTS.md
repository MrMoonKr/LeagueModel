# LeagueModel Asset-System Migration

## 목적

`LeagueLib`와 `Spek`에 의존하는 현재 LoL 에셋 로딩 경로를 앱 내부의
`src/assets/` C++ 구현으로 이전한다. 이전 후에도 현재 뷰어의 캐릭터 로드,
텍스처 표시, 스켈레톤 애니메이션, 스킨 전환은 유지되어야 한다.

`glfw`, `glad`, ImGui, 이미지 디코더, 수학 라이브러리는 렌더링/플랫폼 의존성으로
별도 범위다. 이 마일스톤에서 제거하는 대상은 `ext/leaguelib`가 제공하는 WAD, BIN,
Spek 비동기 파일 시스템이다.

## 앱 관점의 역기획

### 최종 사용자 계약

앱을 실행하면 설정한 League 설치 경로에서 WAD를 인덱싱하고 Jinx를 불러온다. 사용자는
Skins 창에서 다른 스킨을 선택하고, Animations 창에서 클립을 재생하며, Assets 창에서
현재 캐릭터가 실제로 요청·해결·로드한 에셋과 의존 관계를 탐색할 수 있다.

### 이를 위해 앱이 필요로 하는 경계

1. `LeagueModelApp`은 게임 루트를 전달하고 에셋 시스템을 열며, 프레임마다 필요한
   비동기 작업을 진행한다. WAD 내부 형식이나 BIN 구조를 직접 알지 않는다.
2. `Character`는 skin BIN에서 참조를 해석해 SKN, SKL, TEX, animation BIN/ANM을 요청하고
   렌더 가능한 모델 상태를 만든다. 물리 파일 핸들이나 전역 캐시는 소유하지 않는다.
3. `AssetSystem`은 논리 경로 또는 WAD 해시로 엔트리를 해결하고, 원시 바이트와 로드
   기록을 제공한다.
4. 형식별 파서(`WadArchive`, `BinReader`, SKN/SKL/ANM/TEX 로더)는 바이트를 해석할 뿐
   UI나 OpenGL을 알지 않는다. `ManagedImage`만 해석된 TEX 바이트를 OpenGL 텍스처로
   업로드한다.
5. Assets UI는 파서 내부 객체를 추측해 그리지 않는다. `AssetSystem`의 로드 기록과
   `Character`의 참조 관계를 사용해 트리를 만든다.

### LoL 특화 원칙

- WAD 엔트리는 경로가 아닌 64-bit 해시만 가진 경우가 있다. `AssetEntry`는 원본 WAD,
  해시, 오프셋, 압축/원본 크기를 항상 보존하고, `GameHashIndex`로 해석된 경로는 선택
  메타데이터로 보관한다.
- WAD의 압축 방식과 파일 변형은 명시적으로 실패를 기록한다. 알 수 없는 포맷을 빈
  데이터로 취급하거나 다른 파일명으로 추측해 열지 않는다.
- BIN은 단순 키-값 JSON이 아니다. 링크 파일, 해시 키, 배열/맵/객체, animation graph
  자료형을 보존하는 최소 타입 시스템을 마련한 뒤 현재 `Character`가 쓰는 필드만
  단계적으로 지원한다.
- 전역 싱글턴 대신 앱이 소유한 `AssetSystem`을 `Character`에 주입한다. 로드 요청마다
  원본 `AssetEntry`와 결과 상태를 기록해 UI와 오류 보고가 같은 사실을 본다.

## 목표 구조

`src/assets/`의 클래스 이름과 책임은 DOTA2 프로젝트의 에셋 모듈을 기준으로 유지한다.

```
AssetSystem
  AssetArchiveSet (mount priority, entry index)
    WadArchive       (LoL DATA.wad.client)
    DirectoryArchive (추출본/개발 오버라이드)
  AssetRegistry      (raw payload 및 파싱 결과 캐시)
  GameHashIndex      (hash <-> known logical path)
  LoadRecord         (요청/해결/로드/실패 및 dependency edge)
```

공통 값 타입은 `AssetPath`, `AssetEntry`, `AssetData`, `BinaryReader`를 사용한다.
LoL 전용 파서는 `src/assets/formats/`에 둔다. 최소 구성은 `wad`, `bin`, `skn`, `skl`,
`anm`, `tex`다.

## 순차 마일스톤

### M0 — 기준선과 범위 고정

- 현재 `--smoke` 검증을 실행해 Jinx/Ahri/Kassadin 로드 기준을 기록한다.
- `rg`로 `LeagueLib`/`Spek` 사용 지점을 목록화하고, 각 지점을 대체할 새 API에 매핑한다.
- 현재 WAD 인덱스 수, 경로 해시 목록의 상태, 실패한 로드 메시지를 기록한다.

완료 조건: 기존 앱이 빌드되고 smoke 출력이 기준선으로 남아 있다.

### M0 실행 기록 (2026-09-07)

- `cmake --preset vs2022-x64` 및 `cmake --build --preset debug` 성공.
- `config.ini`의 `D:\Riot Games\League of Legends\Game\DATA\FINAL`와 기본 설치 경로
  `C:\Riot Games\League of Legends\Game\DATA\FINAL`는 현재 작업 환경에 없어 smoke는
  실행하지 못했다. 실제 게임 루트가 제공되면 M6 전에 반드시 다시 실행한다.
- 외부 API 사용처: WAD mount/app lifecycle(`LeagueModelApp`, `main`), 파일 핸들 및
  상태(`Skin`, `Skeleton`, `Animation`, `ManagedImage`, UI), BIN/animation graph
  (`Character`, `CharacterAnimation`, `ClipPropertyData`), CMake link.
- 대체 방향: mount/read는 `AssetSystem`, 파일 상태는 `AssetLoadState`, BIN 값 접근은
  `BinDocument`/`BinValue`, 포맷 로더 입력은 `AssetData`로 각각 이전한다.

### M1 — 독립 에셋 코어

- `src/assets/`에 `AssetPath`, `AssetEntry`, `AssetArchive`, `AssetArchiveSet`,
  `AssetRegistry`, `AssetSystem`, `BinaryReader`, `DirectoryArchive`를 C++로 구현한다.
- 경로 정규화, mount priority(last mounted wins), archive 소유권, 잘못된 범위 읽기 실패를
  단위 테스트로 검증한다.
- 이 단계에서는 기존 런타임 로더를 바꾸지 않는다.

완료 조건: 느슨한 디렉터리에서 엔트리 탐색·읽기·중복 우선순위가 재현 가능하다.

### M1 실행 기록 (2026-09-07)

- `src/assets/`에 `AssetPath`, `AssetEntry`, `AssetData`, `AssetArchive`,
  `AssetArchiveSet`, `AssetRegistry`, `AssetSystem`, `DirectoryArchive`,
  `BinaryReader`를 추가했다.
- `LeagueModelAssetTests` CTest는 경로 정규화, last-mounted-wins, raw payload
  읽기, 범위 밖 바이너리 읽기 거부를 검증한다.
- `cmake --build --preset debug` 및 `ctest --test-dir build -C Debug
  --output-on-failure` 통과.

### M2 — LoL WAD 및 해시 인덱스

- `WadArchive`가 `DATA.wad.client`를 안전하게 스캔하고 단일 엔트리를 복호/복원 없이
  원시 payload로 읽을 수 있게 한다.
- 실제 사용하는 압축/중복 블록 형식을 지원하고, 지원하지 않는 형식은 구체적 오류로
  남긴다.
- `GameHashIndex`를 기존 hash-list 데이터에서 구성해 해시와 알려진 경로를 연결한다.
- `AssetArchiveSet`으로 Champions WAD mount 우선순위를 구현한다.

완료 조건: Jinx skin BIN, SKN, SKL, TEX, 하나의 ANM 엔트리를 새 시스템으로 찾아 원시
크기와 해시를 확인할 수 있다.

### M2 실행 기록 (진행 중, 2026-09-07)

- `WadArchive`는 WAD v3 헤더/엔트리 테이블, uncompressed, Zstd, Zstd multi-subchunk
  payload를 구현했다. `GameHashIndex`는 `cache/hashes.game.txt`의 hash/path 양방향
  인덱스를 제공한다.
- 실제 `\\DESKTOP-GAMA3CK\GameHDD\myGames\Riot Games\League of Legends\Game\DATA\FINAL\Champions\Jinx.wad.client`
  검증: 3,735개 엔트리 스캔 및 첫 multi-subchunk 엔트리 21,876바이트 복원 성공.
- 아직 `AssetSystem`의 Champions 디렉터리 자동 mount와 SKN/SKL/TEX/ANM별 실제 엔트리
  검증은 M3 전까지 보완한다. Zlib/기타 미확인 storage type은 의도적으로 오류를 반환한다.

### M3 실행 기록 (진행 중, 2026-09-07)

- `BinDocument`는 실제 Jinx skin0 BIN(v3)의 PROP 헤더, 25개 링크, 16개 root field 및
  중첩 value/object/container/array/map을 자체 파싱한다.
- `ResolveSkinAssets`는 SKN, SKL, TEX 경로와 animation graph hash를 추출했다.
- M4의 포맷 로더 전환 전에 animation graph의 clip/mask/track 값 접근을 같은 value API로
  검증하고, 기존 `Character`의 `LeagueLib::Bin` 직접 접근을 이 resolver로 대체한다.

### M3 — BIN 최소 파서와 캐릭터 참조 해석

- `BinReader`/값 타입으로 skin BIN의 루트 객체, linked files, 문자열, hash, 배열, 맵,
  객체를 지원한다.
- `Character::Load()`의 skin mesh properties, material/material override, animation graph
  참조를 새 BIN API로 전환한다.
- animation graph에 필요한 clip/mask/track/event 데이터만 우선 포팅한다.

완료 조건: 새 API만으로 skin BIN에서 SKN/SKL/TEX와 animation BIN/ANM 경로를 뽑아낸다.

### M4 — 바이너리 모델 포맷 전환

- SKN, SKL, ANM, TEX의 바이트 입력을 `AssetSystem`으로 전환한다.
- 기존 `Skin`, `Skeleton`, `Animation`, `ManagedImage` 클래스 이름과 렌더러 입력 구조는
  유지하되, `Spek::File::Handle` 및 `Spek::File::LoadState`는 자체 `AssetLoadState`로
  교체한다.
- 동기 구현으로 정확성을 먼저 검증하고, 필요한 경우 작업 큐를 도입한다.

완료 조건: `LeagueLib`/`Spek` 없이 Jinx를 텍스처·스켈레톤·애니메이션과 함께 렌더한다.

### M5 — 로드 기록 기반 Assets 트리뷰

- 모든 요청에 `LoadRecord`(entry, 상태, 오류, 부모 요청)를 남긴다.
- Assets 창은 `Character → BIN → SKN/SKL → material/TEX → animation BIN/ANM` 계층과
  미해결 해시, 실패 원인을 표시한다.
- 트리는 실제 로드 기록을 표시하며, UI 전용 재스캔이나 추정 경로를 만들지 않는다.

완료 조건: 선택한 스킨의 모든 요청 에셋과 실패/미해결 항목을 한 트리에서 추적한다.

### M6 — 의존성 제거 및 회귀 검증

- `src`와 CMake에서 `LeagueLib`/`Spek` include·link·subdirectory를 제거한다.
- 필요한 독립 수학 라이브러리 의존성은 `ext/leaguelib` 밖으로 명시적으로 이전한다.
- Debug/Release 빌드와 Jinx/Ahri/Kassadin smoke를 통과시킨다.

완료 조건: 실행 파일의 asset pipeline이 `ext/leaguelib` 없이 빌드·실행되고, M0 기준선의
기능이 유지된다.

## 작업 규칙

- 마일스톤을 건너뛰지 않는다. 새 WAD/BIN 경로가 검증되기 전 기존 파서를 삭제하지 않는다.
- 포맷 사양, 오프셋, 해시 알고리즘은 샘플 파일과 테스트로 확인한다.
- 파싱 오류는 예외/오류 결과와 `LoadRecord`에 남기고 앱 전체를 종료시키지 않는다.
- 포맷별 코드가 UI, OpenGL, `LeagueModelApp` 헤더를 include하지 않게 한다.
- 각 마일스톤 완료 시 변경 파일, 검증 명령, 알려진 미지원 포맷을 기록한다.

## 검증 명령

```
cmake --preset vs2022-x64
cmake --build --preset debug
bin/Debug/LeagueModel.exe <League DATA/FINAL 경로> --smoke
```

Windows에서는 실제 League 설치 경로 또는 `config.ini`의 `root`가 필요하다. smoke는
Jinx, Ahri, Kassadin, Jinx 순으로 모델 로드와 OpenGL 오류를 확인한다.
