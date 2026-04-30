# Transform Refactor Execution Plan

## 1. Goal

`Engine::Transform`를 현재의 혼합형 구현에서 벗어나, `DX11_Personal_Portfolio`의 `CTransformCom`처럼:

- `SimpleMath` 계열 타입(`Vec3`, `Quat`, `Matrix`)만 주 경로로 사용하고
- 로컬 TRS와 회전/스케일 불변식을 `Transform` 내부 정본으로 유지하며
- 기존 프로젝트 호출부는 가능한 한 깨지지 않게 래퍼 API와 `deprecated` 경고로 완충하는

구조로 전면 리팩토링한다.

이번 문서의 목적은 "바로 구현"이 아니라, 어떤 것을 정본으로 삼고 어떤 API를 살아남게 할지 먼저 고정하는 것이다.

## 2. Current Comparison

### 2.1 Current `Engine::Transform`

현재 `Transform`는 표면적으로는 로컬 기반 API를 제공하지만, 실제로는 world basis 직접 수정과 다시 분해(`Decompose`)하는 경로가 섞여 있다.

핵심 관찰:

- `TRANSFORM_DESC`가 `position`, `scale`, `pitch/yaw/roll`까지 들고 있어 초기화 단계부터 authoring TRS를 받는다.  
  참조: [Transform.h](/d:/cpp_ksh/Jusin158/DX11/Expedition_158/Engine/Public/Transform.h:12)
- 내부 정본은 `Vec3 _localPosition`, `Quat _localRotation`, `Vec3 _localScale`처럼 보이지만, `Set_State()`가 world matrix를 수정한 뒤 `Decompose_WorldMatrix()`로 다시 로컬 상태를 복원하는 구조다.  
  참조: [Transform.cpp](/d:/cpp_ksh/Jusin158/DX11/Expedition_158/Engine/Private/Transform.cpp:45), [Transform.cpp](/d:/cpp_ksh/Jusin158/DX11/Expedition_158/Engine/Private/Transform.cpp:320)
- `Get_State()`도 `_worldMatrix`에서 basis/position을 직접 읽는다.  
  참조: [Transform.cpp](/d:/cpp_ksh/Jusin158/DX11/Expedition_158/Engine/Private/Transform.cpp:26)
- `Get_Scaled()`는 `XMFLOAT3`를 반환한다.  
  참조: [Transform.h](/d:/cpp_ksh/Jusin158/DX11/Expedition_158/Engine/Public/Transform.h:56)
- `Get_WorldMatrixPtr()`는 `Matrix`를 `XMFLOAT4X4*`로 재해석한다.  
  참조: [Transform.cpp](/d:/cpp_ksh/Jusin158/DX11/Expedition_158/Engine/Private/Transform.cpp:85)

즉, 겉 API는 modernized 되어 있지만 내부 계약은 아직 "임의 basis/world matrix 수정도 허용하는 구형 transform"에 가깝다.

### 2.2 Reference `CTransformCom`

포트폴리오 쪽 `CTransformCom`는 다음 성격을 가진다.

- 정본을 `m_matLocal + m_vLocalSignedScale + rotation cache`로 둔다.  
  참조: [CTransformCom.h](</d:/cpp_ksh/Jusin158/DX11/DX11_Personal_Portfolio/Engine/Public/CTransformCom.h:157>)
- world matrix는 파생 캐시이며 dirty/revision 기반으로 갱신한다.  
  참조: [CTransformCom.cpp](</d:/cpp_ksh/Jusin158/DX11/DX11_Personal_Portfolio/Engine/Private/CTransformCom.cpp:658>)
- 회전/스케일 불변식을 깨뜨릴 수 있는 `Set_WorldMatrix`, `Set_Right`, `Set_Up`, `Set_Look`는 `deprecated`로 몰아둔다.  
  참조: [CTransformCom.h](</d:/cpp_ksh/Jusin158/DX11/DX11_Personal_Portfolio/Engine/Public/CTransformCom.h:64>)
- 안전한 authoring 경로를 `Set_Position`, `Set_Scale`, `Set_RotationEuler`, `Set_RotationQuaternion` 중심으로 유도한다.  
  참조: [CTransformCom.h](</d:/cpp_ksh/Jusin158/DX11/DX11_Personal_Portfolio/Engine/Public/CTransformCom.h:75>)

이 구조가 이번 이식의 기준선이다.

## 3. Problems To Solve In This Repo

### 3.1 Type policy is not strict enough

사용자 의도는 "`XMFLOAT3` 같은 SimpleMath가 아닌 녀석들은 Transform 사용 경로에서 배제"다.

현재 남아 있는 문제:

- `Get_Scaled()`가 `XMFLOAT3` 반환
- `Get_WorldMatrixPtr()`가 `XMFLOAT4X4*` 반환
- `Set_State(State, FXMVECTOR)`처럼 DirectXMath raw vector 입력이 public 주 경로

목표는 "엔진 내부 정본/주 API는 `Vec3`, `Quat`, `Matrix`"로 고정하고, raw DirectX 타입은 바인딩/레거시 호환층으로만 제한하는 것이다.

### 3.2 Unsafe mutation paths are mixed with safe TRS paths

현재는 아래가 동시에 열려 있다.

- `Set_LocalPosition`, `Set_LocalRotation`, `Set_LocalScale`
- `Set_State`
- world matrix 분해 기반 복원

이 상태에서는 호출자가 어떤 경로가 안전한지 알기 어렵고, signed scale / 축 정규화 / 회전 캐시 정책도 일관되게 유지하기 어렵다.

### 3.3 Existing gameplay/editor call sites already depend on old names

현재 호출부는 `Set_State(State::Position, XMVectorSet(...))`, `Turn(Get_State(State::Right), ...)` 같은 식으로 이미 퍼져 있다.  
참조 예시: [Weapon.cpp](/d:/cpp_ksh/Jusin158/DX11/Expedition_158/Client/Private/Weapon.cpp:37), [FreeCamera.cpp](/d:/cpp_ksh/Jusin158/DX11/Expedition_158/Client/Private/FreeCamera.cpp:51)

따라서 "좋은 API로 갈아타기"와 "기존 프로젝트 사용자가 헷갈리지 않게 하기"를 동시에 만족하려면, 이름은 유지하되 내부 구현과 권장 경로를 교체하는 단계적 이행이 필요하다.

## 4. Target Design

## 4.1 Source of truth

리팩토링 후 `Transform`의 정본은 아래처럼 잡는 것이 목표다.

- `Matrix _localMatrix`
- `Vec3 _localSignedScale`
- `Quat _localRotationCache`
- `Vec3 _localEulerDegreeCache`
- `Matrix _worldMatrix`
- dirty mask / revision tracking

즉, 현재의 `_localPosition`, `_localRotation`, `_localScale` 조합을 그대로 유지하는 대신, 포트폴리오 쪽처럼 "local basis를 직접 가진 matrix + 파생 cache" 구조로 이동한다.

이렇게 해야 하는 이유:

- deprecated 경로(`Set_State`, `Set_WorldMatrix`)로 들어온 basis 수정도 내부적으로 수용 가능
- signed scale 보존 정책을 세우기 쉬움
- 미래에 parent hierarchy를 붙일 때 확장성이 좋음
- `LookAt`, `Rotate`, `Scale`가 모두 동일한 정본을 만지게 됨

### Member migration map

현재 `Transform` 멤버와 이식 후 목표 멤버의 관계는 아래처럼 정리한다.

| Current `Transform` member | Fate | Notes |
| --- | --- | --- |
| `_localPosition` | 제거 | 위치 정본은 `local matrix`의 translation이 담당한다. 별도 멤버를 두지 않는다. |
| `_localScale` | 제거 | 스케일 정본은 `signed scale`이 담당한다. 별도 scale 멤버는 중복 상태가 된다. |
| `_localRotation` | 현재 형태로는 제거 | 회전은 `quat cache`로는 유지할 수 있지만, 독립 정본 멤버로는 두지 않는다. |
| `_localEulerAngles` | 캐시로 격하 | 에디터/직관적 authoring을 위해 유지 가능하지만, `local matrix`에서 동기화되는 파생 캐시로만 둔다. |
| `_worldMatrix` | 유지 | world transform 파생 캐시로 유지한다. |
| `_isDirty` | 제거 | 단일 bool 대신 dirty mask(`ROTATION`, `WORLD`)로 치환한다. |
| `_speedPerSec` | 유지 | runtime tuning 값으로 유지한다. |
| `_radianPerSec` | 유지 | runtime tuning 값으로 유지한다. |

즉 현재 `Transform`은 여러 값을 병렬 정본처럼 들고 있는 구조인데, 리팩토링 후에는 그 병렬 상태를 정리한다.

- 정본: `local matrix + signed scale`
- 파생 캐시: `rotation quaternion + rotation euler + world matrix`
- 보조 상태: dirty mask / revision / speed tuning

### Cache policy

`_localPosition`, `_localRotation`, `_localScale`를 모두 남겨두고 "캐시로만 쓰자"는 방향은 권장하지 않는다.

이유:

- 캐시라고 해도 setter 경로가 많아지면 곧 사실상 또 다른 정본처럼 취급되기 쉽다
- 특히 position/scale은 `local matrix`와 `signed scale`에서 곧바로 복원 가능한 값이라 중복 저장 이득이 작다
- 지금 구조가 복잡해진 원인 자체가 병렬 상태 보관과 world-decompose 복원 경로의 혼합이다

따라서 권장 정책은 아래와 같다.

- `_localPosition`: 캐시로도 두지 않음
- `_localScale`: 캐시로도 두지 않음
- `_localRotation`: 독립 정본으로 두지 않음
- `quat cache`: 유지 가능
- `euler cache`: 유지 가능

한 줄 요약:

`CTransformCom` 쪽 멤버 변수는 대부분 이식하되, 회전 캐시를 제외한 나머지 로컬 값들은 "별도 로컬 멤버"가 아니라 `local matrix`와 `signed scale`로 흡수한다.

## 4.2 `TRANSFORM_DESC` ownership

`TRANSFORM_DESC`는 계속 `Transform` 내부 중첩 타입으로 유지한다.

이유:

- 현재 `GameObject::GAMEOBJECT_DESC`가 `Transform::TRANSFORM_DESC`를 상속하고 있다.  
  참조: [GameObject.h](/d:/cpp_ksh/Jusin158/DX11/Expedition_158/Engine/Public/GameObject.h:12)
- transform authoring 파라미터의 소유권은 여전히 `Transform` 자신에게 있는 편이 맞다.

다만 내용은 재정리 대상이다.

권장 방향:

- 유지: `speedPerSec`, `degreePerSec`
- 유지: `position`, `scale`
- 유지 가능: `pitch`, `yaw`, `roll`
- 추가 후보: `Quat rotationQuaternion`
- 정책: Euler authoring과 Quaternion authoring이 동시에 올 수 있다면 우선순위를 명시해야 함

1차 구현에서는 기존 호환성을 위해 기존 필드명을 유지한다.  
2차에서 필요하면 quaternion 기반 desc 확장을 추가한다.

## 4.3 Public API split

### Safe primary API

이쪽이 앞으로의 정식 사용 경로다.

- `Get_State(State)`, `Set_State(State, Vec3)`를 팀 공용 축/위치 접근 표면으로 사용
- `Get_Position()`, `Set_Position(const Vec3&)`
- `Get_Scale()`, `Set_Scale(const Vec3&)`
- `Get_RotationEuler()`, `Set_RotationEuler(const Vec3&)`
- `Get_RotationQuaternion()`, `Set_RotationQuaternion(const Quat&)`
- `Scale(...)`, `Go_*`, `Rotate(...)`, `LookAt(...)`

정책적으로 `Get_Right()`, `Get_Up()`, `Get_Look()`, `Set_Right()` 같은 개별 basis 함수는 팀 공용 주 API로 승격하지 않는다.

이유:

- 현재 프로젝트는 이미 `State` enum 기반 접근 문법이 퍼져 있다
- 축별 getter/setter를 따로 늘리면 팀 기준 표면적이 불필요하게 커진다
- 개인 엔진에서는 편할 수 있지만, 팀 프로젝트에서는 "`State` 기반 한 가지 방식"으로 통일하는 편이 읽기와 리뷰가 쉽다

### Compatibility wrapper API

기존 이름은 당장 삭제하지 않고, 내부적으로 safe API를 호출하는 thin wrapper로 남긴다.

예상 매핑:

- `Get_LocalPosition()` -> `Get_Position()`
- `Set_LocalPosition(...)` -> `Set_Position(...)`
- `Get_LocalScale()` -> `Get_Scale()`
- `Set_LocalScale(...)` -> `Set_Scale(...)`
- `Get_LocalEulerAngles()` -> `Get_RotationEuler()`
- `Set_LocalRotation(float pitch, float yaw, float roll)` -> `Set_RotationEuler(Vec3{ pitch, yaw, roll })`
- `Set_LocalRotation(const Quat&)` -> `Set_RotationQuaternion(...)`
- `Add_LocalRotation(...)` -> `Rotate(...)` 또는 quaternion delta 적용 helper
- `Get_WorldForward/Right/Up()` -> `Get_State(State::Look/Right/Up)` 기반 normalized wrapper

즉, 외부 이름은 살리되 내부 정본 조작 함수는 축소한다.

## 4.4 Deprecated API policy

사용자 요청대로 아래는 "남겨두되 막는" 방향이 맞다.

- `Set_WorldMatrix`
- raw/legacy overloading 형태의 `Set_State`
- basis 직접 setter (`Set_Right`, `Set_Up`, `Set_Look`)는 새 public API로 추가하지 않음

정책:

- 존재는 시킨다
- `[[deprecated(...message...)]]`를 붙인다
- 내부적으로는 가능한 한 정본 불변식을 복원하도록 작성한다
- 문서/호출부에서는 사용 금지 경로로 간주한다

권장 deprecated 메시지:

- `Set_WorldMatrix`: arbitrary world matrix import is unsafe for authoring. Use Position/Rotation/Scale setters.
- `Set_State(FXMVECTOR ...)`: raw DirectXMath state mutation is legacy-only. Prefer `Vec3`-based state/TRS APIs.

## 4.5 SimpleMath-only rule

최종적으로 `Transform` public surface에서 허용할 타입 정책:

- 허용: `Vec3`, `Quat`, `Matrix`
- 제한 허용: `FXMVECTOR`, `XMFLOAT4X4*`는 셰이더 바인딩/외부 인터op/레거시 wrapper에서만
- 금지 목표: `XMFLOAT3` 반환형/주 입력형

구체적으로는:

- `Get_Scaled()`는 제거 후보이거나 최소한 deprecated `Vec3 Get_Scale()` wrapper로 대체
- `Get_WorldMatrixPtr()`는 유지하더라도 shader/legacy interop 전용임을 명시
- 새 코드에서는 `XMVectorSet(...)`를 만들어 넘기지 않도록 `Vec3` 오버로드를 우선 제공

## 5. Migration Strategy

## 5.1 Phase A - Internal re-foundation

먼저 내부 정본을 바꾼다.

- `Transform` 내부 저장 구조를 `CTransformCom` 계열로 전환
- dirty/world cache/revision 도입
- rotation/euler/signed-scale 동기화 규칙 이식
- public API 시그니처는 최대한 유지

이 단계 목표는 "겉모습은 비슷하지만 내부는 안전해진 Transform"이다.

## 5.2 Phase B - Compatibility wrappers

기존 이름을 살리되 새 정본으로 연결한다.

- `Get_State`, `Set_State`
- `Set_LocalPosition`, `Set_LocalRotation`, `Set_LocalScale`
- `Get_WorldForward/Right/Up`
- `SetUp_Scale`, `Scaling`, `Rotation`, `Turn`

이 단계에서 중요한 점:

- wrapper는 기능을 제공하되 새로운 정본을 우회하면 안 된다
- wrapper 내부에서 world matrix를 직접 만지는 구현은 금지

## 5.3 Phase C - Deprecation and call-site cleanup

이후 실제 사용처를 점진적으로 정리한다.

우선 정리 대상:

- `Set_State(State::Position, XMVectorSet(...))`
- `Turn(Get_State(State::Right), ...)`처럼 raw axis getter에 의존한 코드
- `Get_Scaled()` 같은 `XMFLOAT3` 반환 경로

치환 원칙:

- 위치 설정: `Set_Position(Vec3{...})`
- 회전 축 조회: `Get_State(State::Right/Up/Look)`
- 회전 설정: `Set_RotationEuler`, `Set_RotationQuaternion`

## 5.4 Phase D - Optional hierarchy extension

포트폴리오 쪽의 `Set_Parent()`까지 바로 들여올지는 별도 결정이 필요하다.

이번 리팩토링의 핵심은 타입 정책과 정본 재정립이지, hierarchy까지 한 번에 여는 것이 아니다.  
따라서 parent 지원은 문서상 "확장 후보"로 두고 1차 범위에서는 제외하는 것이 안전하다.

## 6. Concrete Refactor Decisions

## 6.1 Keep class name `Transform`

이 프로젝트에서는 이미 `Shared<Transform>`가 엔진 전역 표면에 퍼져 있다.  
따라서 클래스명은 `Transform` 그대로 유지하고, 내부 구조와 API 구성을 `CTransformCom` 스타일로 이식한다.

## 6.2 Keep existing enum `State`

현재 프로젝트는 `Engine_Enum.h`의 `State`를 사용한다.  
참조: [Engine_Enum.h](/d:/cpp_ksh/Jusin158/DX11/Expedition_158/Engine/Public/Engine_Enum.h:11)

이 enum은 유지하고, 축/위치 접근의 대표 표면도 `Get_State()` / `Set_State()` 계열로 통일한다.

- 팀 공용 axis/position 접근: `Get_State(State::...)`
- 팀 공용 axis/position 설정: `Set_State(State::..., Vec3)`
- 의미가 강한 authoring API는 별도 유지: `Get_Position`, `Set_Position`, `Get_Scale`, `Set_Scale`, `Set_RotationEuler`, `Set_RotationQuaternion`

즉 `Get_State()`는 compatibility-only가 아니라, 이 프로젝트에서 계속 살아남을 공용 enum 기반 표면으로 본다.  
단, `FXMVECTOR` 기반 raw 입력 버전은 레거시 호환층으로 내리고 `Vec3` 기반 overload를 정식 경로로 승격한다.

## 6.3 Do not use raw world decomposition as the normal authoring path

현재 구현의 가장 큰 구조 문제는 "`Set_State` -> world 수정 -> `Decompose_WorldMatrix`"가 authoring 경로 안에 들어와 있다는 점이다.

리팩토링 후 원칙:

- normal path에서는 `Decompose`에 의존하지 않는다
- `Decompose`는 legacy import 또는 truly unsafe recovery 경로에만 남긴다
- transform authoring은 항상 정본 로컬 상태를 통해 이뤄진다

## 6.4 `Set_WorldPosition()` semantics must be clarified

현재는 parent 개념이 없어서 `Set_WorldPosition()`이 사실상 `Set_LocalPosition()`과 동일하다.  
참조: [Transform.cpp](/d:/cpp_ksh/Jusin158/DX11/Expedition_158/Engine/Private/Transform.cpp:151)

리팩토링 문서 기준:

- 1차 범위에서는 동일 semantics 유지
- 단, 이름상 오해가 있으므로 주석 또는 문서로 "non-hierarchical transform에서는 local == world"를 명시
- future hierarchy를 붙일 경우, 이 함수는 실제 world 보존 setter로 재정의

## 7. Implementation Checklist

- `docs/RIGHTNOW` 문서 승인
- `Transform` 헤더에 safe API / compatibility API / deprecated API 구획 재정리
- `Transform` 내부 저장 구조를 local matrix + signed scale + caches로 교체
- `Get_State` / `Set_State`를 새 정본 위 thin wrapper로 재작성
- `Get_Scaled()` 제거 또는 deprecated 처리
- `Vec3` 중심 오버로드 추가
- call site 1차 치환: position set / axis access
- 빌드 확인
- behavior smoke test: 이동, 회전, `LookAt`, scale 유지, camera, weapon, sky

## 8. Risks

- 현재 호출부 일부는 `Set_State`의 느슨한 동작에 은근히 의존할 수 있다
- mirrored scale / signed scale 보존은 이식 과정에서 가장 깨지기 쉬운 부분이다
- `Get_WorldMatrixPtr()`의 ABI/메모리 레이아웃 가정은 조심해야 한다
- hierarchy를 아직 도입하지 않은 상태에서 `world`라는 이름이 과하게 남아 있으면 오해를 유지할 수 있다

## 9. Recommended First Implementation Slice

가장 안전한 1차 구현 범위:

1. `Transform` 내부 정본을 `CTransformCom` 스타일로 교체
2. `TRANSFORM_DESC`는 nested type 유지
3. `Set_WorldMatrix`, `Set_State`는 `deprecated` 처리
4. `Get_Position/Set_Position`, `Get_Scale/Set_Scale`, `Get_RotationEuler/Quaternion`를 정식 API로 승격
5. 기존 호출부는 최대한 wrapper로 살리고, 아주 위험한 사용처만 선택적으로 치환

이 범위면 사용자의 목표였던:

- SimpleMath 중심화
- 기존 사용자 혼란 최소화
- 내부 전면 리팩토링
- unsafe API 차단

을 한 번에 묶어서 진행할 수 있다.

## 10. Open Questions Before Implementation

구현 들어가기 전에 한 번 더 정해야 할 항목:

1. `Transform`에 parent hierarchy까지 이번 턴에서 같이 넣을지
2. `TRANSFORM_DESC`에 quaternion authoring 필드를 바로 추가할지
3. `Get_WorldMatrixPtr()`를 유지할지, 아니면 `const Matrix&` 사용을 더 강하게 밀지
4. `Set_Right/Set_Up/Set_Look` 같은 축별 setter를 둘지, 아니면 enum 기반 `Set_State` 하나로 통일할지

현재 판단으로는 1차 구현에서:

- parent는 제외
- quaternion desc는 보류
- `Get_WorldMatrixPtr()`는 호환성 때문에 유지
- basis setter는 새로 늘리지 않고 enum 기반 `Get_State` / `Set_State(Vec3)`로 통일

이 가장 안전하다.
