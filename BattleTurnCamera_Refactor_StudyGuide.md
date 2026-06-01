# BattleTurnCamera Refactor Study Guide

## 1. 이번 리팩터링의 목표

이번 작업의 핵심 목표는 `BattleTurnCamera` 주변에 흩어져 있던 **중복 계산**과 **중복 분기**를 줄이고,

- 계산은 shared helper에서
- 적용은 runtime/editor owner가
- mode별 preset/clip 선택은 한 곳에서

보이도록 구조를 정리하는 것이었습니다.

이번에 손댄 주요 파일은 아래 4개입니다.

- `Client/Public/BattleTurnCameraPoseUtils.h`
- `Client/Public/BattleTurnCamera.h`
- `Client/Private/BattleTurnCamera.cpp`
- `MainEditor/Private/BattleCameraPreview_View.cpp`

---

## 2. 리팩터링 전 어떤 문제가 있었나

### 2-1. `WIDE` 카메라 계산이 여러 군데에 있었다

`WIDE` 모드 카메라 위치/바라보는 점 계산이 아래처럼 여러 군데에 흩어져 있었습니다.

- `BattleTurnCamera::Focus_Battle(...)`
- `BattleTurnCamera::Evaluate_ShotRequestPose(...)`
- `BattleCameraPreview_View::Apply_BattleCameraPreviewPose(...)`
- `BattleCameraPreview_View::Evaluate_PreviewModePose(...)`

문제는 단순히 "복붙이 많다"가 아닙니다.

- runtime과 preview가 서로 다른 숫자를 쓰고 있었습니다.
- 어떤 곳은 clip offset을 적용하고, 어떤 곳은 적용하지 않았습니다.
- 어떤 곳은 FOV를 건드리고, 어떤 곳은 안 건드렸습니다.

이 상태에서는 나중에 `WIDE` 구도를 수정하려고 할 때:

1. 한 군데만 고친다
2. runtime과 preview가 달라진다
3. 다시 추적하느라 시간이 든다

라는 문제가 반복됩니다.

### 2-2. 평가 함수가 실제 카메라를 만질 위험이 있었다

`Evaluate_ShotRequestPose(...)`는 이름상 **pose를 계산하는 함수**입니다.
그런데 이런 함수 안에 실제 transform 적용 코드가 들어가면 함수 역할이 섞입니다.

좋지 않은 이유:

- transition 중간 계산에서도 호출될 수 있음
- preview parity 확인할 때 "계산만 했는지, 실제 적용까지 했는지" 헷갈림
- 테스트/디버깅이 어려워짐

즉, `Evaluate_*` 함수는 가능한 한 **pure evaluator**처럼 유지하는 것이 좋습니다.

### 2-3. mode별 preset/clip 선택 로직이 `switch`에 길게 늘어져 있었다

기존 `Apply_ShotRequest(...)`에는 mode별로

- 어떤 preset을 쓸지
- 어떤 clip을 쓸지
- `PLAYERATTACK`에서는 `skillId` fallback을 어떻게 할지

를 한 번에 다 처리하는 큰 `switch`가 있었습니다.

이런 구조는 기능은 동작해도,

- "선택 로직"
- "상태 전이 로직"

이 섞여 있어서 읽기가 어렵습니다.

---

## 3. 이번에 어떻게 구조를 바꿨나

## 3-1. `WIDE` 계산을 shared helper 하나로 모았다

새로 추가한 핵심 helper:

```cpp
inline bool Evaluate_BattleTurnWidePose(
    const Vec3& heroCenter,
    float yawDegrees,
    float fovY,
    BattleTurnCameraEvaluatedPose& outPose)
```

위 helper는 `Client/Public/BattleTurnCameraPoseUtils.h`에 추가했습니다.

이 helper는 입력을 최소화했습니다.

- `heroCenter`
- `yawDegrees`
- `fovY`

그리고 출력은 `BattleTurnCameraEvaluatedPose` 하나입니다.

이렇게 하면 좋은 점:

- runtime과 preview가 완전히 같은 수학을 쓸 수 있음
- `WIDE` 구도 변경 시 한 군데만 고치면 됨
- 계산 함수 이름과 책임이 명확함

### 현재 helper 핵심 코드

```cpp
outPose.cameraPos =
    heroCenter +
    backDir * 8.5f +
    rightDir +
    Vec3(0.f, 3.f, 2.f);
outPose.lookTarget = heroCenter + Vec3(0.f, 2.2f, 0.f);
outPose.fovY = fovY;
outPose.rollOffset = 0.f;
```

포인트:

- "숫자를 어디에서 쓰는가"를 통일했다
- `lookTarget`, `fovY`, `rollOffset`도 같이 반환한다
- owner는 이 결과를 그냥 적용만 하면 된다

---

## 3-2. runtime 내부에서 `WIDE` 수학 중복을 없앴다

### 변경 전

runtime 안에서 `WIDE` 계산이 최소 2번 있었습니다.

- `Focus_Battle(...)`
- `Evaluate_ShotRequestPose(...)`

### 변경 후

runtime 쪽에서는 `Evaluate_WidePose(...)`를 통해 공통 입력만 모으고,
실제 수학은 shared helper로 넘깁니다.

```cpp
bool BattleTurnCamera::Evaluate_WidePose(float fovY, EvaluatedPose& outPose) const
{
    ...
    const Vec3 baseRot = yawBaseActor->Get_Transform()->Get_RotationEuler();
    return Evaluate_BattleTurnWidePose(heroCenter, baseRot.y, fovY, outPose);
}
```

`Evaluate_ShotRequestPose(...)`의 `WIDE` 분기는 이제 직접 수학을 하지 않고:

```cpp
case BattleCameraMode::WIDE:
    return Evaluate_WidePose(preset.camera.fovy, outPose);
```

처럼 바뀌었습니다.

`Focus_Battle(...)`도 마찬가지로 직접 계산하지 않고:

```cpp
EvaluatedPose pose{};
if (!Evaluate_WidePose(_currentPreset.camera.fovy, pose))
    return;

Apply_EvaluatedPose(pose);
```

처럼 바뀌었습니다.

### 왜 `_currentPreset.camera.fovy`를 쓰는가

처음에는 `_wideModePreset.camera.fovy`를 쓰면 될 것처럼 보일 수 있습니다.
하지만 `WIDE`도 `presetId`로 다른 preset이 들어올 수 있기 때문에,
현재 실제로 선택된 preset의 FOV를 따라야 합니다.

이 포인트는 실무에서 자주 나오는 실수입니다.

- "mode 기본값"과
- "현재 선택된 값"

은 다를 수 있습니다.

---

## 3-3. `Evaluate_ShotRequestPose(...)`를 계산 전용으로 유지했다

이번 작업에서는 evaluator 함수가 실제 transform을 직접 건드리지 않도록 정리했습니다.

좋은 규칙은 아래 한 줄로 요약할 수 있습니다.

> `Evaluate_*`는 계산만, `Apply_*`는 적용만 한다.

현재 runtime owner 경계는 이렇게 읽으면 됩니다.

- `Evaluate_ShotRequestPose(...)` -> pose 계산
- `Apply_EvaluatedPose(...)` -> 카메라에 실제 적용
- `Apply_LookAt(...)` -> transform에 직접 반영

이 경계가 중요한 이유:

- transition 중간값 계산이 안전해짐
- preview에서도 같은 evaluator를 믿고 재사용하기 쉬움
- 버그가 났을 때 "계산이 틀렸는지 / 적용이 틀렸는지" 분리해서 볼 수 있음

---

## 3-4. `EvtBattlePresentation -> BattleShotRequest` 매핑을 별도 helper로 뺐다

기존에는 event subscription lambda 내부에 긴 `switch`가 그대로 있었습니다.

지금은 `cpp` 내부 local helper로 옮겼습니다.

```cpp
BattleTurnCamera::BattleShotRequest Build_BattleShotRequestFromEvent(
    const EvtBattlePresentation& event)
```

이 함수가 하는 일:

- `mode`
- `subject`
- `opponent`
- `skillId`
- `presetId`
- `clipId`
- `useClip`
- `restartClip`

를 request로 옮깁니다.

그리고 `SELECTITEM`, `SELECTSKILL`만

```cpp
request.freezeSubjectDuringTransition = true;
```

를 별도로 세팅합니다.

### 왜 header가 아니라 `cpp` local helper로 뺐나

이건 좋은 학습 포인트입니다.

처음에는 클래스 private 멤버로 빼고 싶을 수 있습니다.
하지만 그 경우 header에 `EvtBattlePresentation` 타입 의존성이 새로 생깁니다.

이번 작업에서는 이 매핑이 **파일 내부 구현 detail** 이기 때문에,
header를 더럽히지 않고 `cpp` 내부 helper로 두는 쪽이 더 적절했습니다.

즉:

- 외부 계약이면 header
- 내부 구현 detail이면 `cpp`

로 생각하면 좋습니다.

---

## 3-5. mode별 preset/clip 선택을 `Resolve_ShotAssets(...)`로 모았다

이번 리팩터링에서 가장 구조적으로 중요한 변화 중 하나입니다.

새 구조:

```cpp
struct ResolvedShotAssets
{
    BattleCameraPreset preset{};
    BattleCameraClip* clip = nullptr;
};
```

그리고:

```cpp
ResolvedShotAssets Resolve_ShotAssets(
    const BattleShotRequest& request,
    const BattleCameraPreset* resolvedPresetById,
    const BattleCameraClip* resolvedClipById) const;
```

이 helper 안에서:

- `presetId`가 있으면 그걸 우선 사용
- 없으면 mode 기본 preset 사용
- `PLAYERATTACK`면 `skillId` fallback 사용
- clip도 같은 방식으로 해석

하게 만들었습니다.

### `Apply_ShotRequest(...)`가 왜 더 읽기 쉬워졌나

예전에는 `Apply_ShotRequest(...)` 안에

- 큰 `switch`
- anchor reset
- transition 시작 여부
- clip restart 여부

가 다 섞여 있었습니다.

지금은 최소한 "어떤 preset/clip을 선택할지"는 한 번에 읽을 수 있습니다.

```cpp
ResolvedShotAssets resolvedAssets =
    Resolve_ShotAssets(request, resolvedPresetById, resolvedClipById);

BattleCameraClip* nextActiveClip = resolvedAssets.clip;
BattleCameraPreset nextPreset = resolvedAssets.preset;
```

이런 리팩터링은 코드를 짧게 만드는 것보다 더 중요합니다.

핵심은:

- 함수가 하나의 질문에 답하게 만드는 것

입니다.

여기서는 `Resolve_ShotAssets(...)`가

> "이 request에서 실제로 사용할 preset/clip은 무엇인가?"

라는 질문 하나에만 답합니다.

---

## 3-6. preset/clip storage 접근 분기도 공통 accessor로 모았다

기존에는 아래 함수들이 각각 자기 `switch`를 갖고 있었습니다.

- `Get_Preset(...)`
- `Set_Preset(...)`
- `Get_MutablePreset(...)`
- `Get_MutableClip(...)`

이번에는 storage 접근을 아래 helper로 공통화했습니다.

```cpp
BattleCameraPreset* Find_PresetStorage(BattleCameraMode cameraMode);
const BattleCameraPreset* Find_PresetStorage(BattleCameraMode cameraMode) const;

BattleCameraClip* Find_ClipStorage(BattleCameraMode cameraMode);
const BattleCameraClip* Find_ClipStorage(BattleCameraMode cameraMode) const;
```

그 결과 public method들은 훨씬 얇아졌습니다.

예를 들어 `Get_Preset(...)`는:

```cpp
if (const BattleCameraPreset* preset = Find_PresetStorage(mode))
    return *preset;

return _wideModePreset;
```

처럼 읽힙니다.

### 이 리팩터링의 장점

- mode -> storage 매핑 규칙이 한 군데로 모임
- 나중에 mode 추가/삭제 시 수정 지점이 줄어듦
- public API가 “저장소 탐색” 대신 “정책 호출”처럼 보임

---

## 3-7. 의미 없는 1-line wrapper를 제거했다

제거한 함수들:

- `Update_WideMode(...)`
- `Update_PlayerFocusMode(...)`
- `Update_EnemyFocusMode(...)`
- `Update_PlayerAttackMode(...)`

왜 제거했나:

이 함수들은 사실상 대부분 `Update_FromCurrentPreset(...)` 한 줄 호출이었습니다.

이런 wrapper는 겉보기엔 분리 같지만,
실제로는:

- 추상화 이득이 거의 없고
- 읽는 사람만 파일을 더 많이 왔다 갔다 하게 만들고
- 나중에 "여기만 특별한가?"라는 착시를 줍니다

그래서 이번에는 `Update()` switch에서 바로

- `WIDE`는 별도 처리
- `ENEMYATTACK`는 별도 처리
- 나머지는 `Update_FromCurrentPreset(...)`

구조로 정리했습니다.

이건 "모든 함수를 작게 쪼개라"가 항상 정답이 아니라는 좋은 예시입니다.

---

## 3-8. preview도 runtime과 같은 `WIDE` 수학을 쓰도록 맞췄다

이번 작업에서 preview 쪽도 같이 정리했습니다.

바뀐 곳:

- `BattleCameraPreview_View::Apply_BattleCameraPreviewPose(...)`
- `BattleCameraPreview_View::Evaluate_PreviewModePose(...)`

이전 preview는 `WIDE`에서 runtime과 다른 오프셋을 쓰고 있었습니다.

예전 preview 쪽 특징:

- `rightDir * 2.0f`
- `Vec3(2.f, 3.f, -2.f)`
- clip offset 일부 반영

반면 runtime `WIDE`는 다른 값을 쓰고 있었습니다.

이번에는 preview도 다음 helper를 쓰게 만들었습니다.

```cpp
Client::Evaluate_BattleTurnWidePose(heroCenter, yawBaseSlot->rotation.y, preset.camera.fovy, pose)
```

### 이 변화의 의미

이건 "코드 예쁘게 하기"가 아니라 **authoring 신뢰성** 문제입니다.

camera tool에서 가장 위험한 상황은:

> preview에서는 좋아 보이는데 runtime에서는 다르게 나온다

입니다.

그래서 preview와 runtime은 가능한 한 같은 evaluator를 써야 합니다.

---

## 4. 삭제된 API

이번에 제거한 preview 전용 public evaluator:

- `BattleTurnCamera::Evaluate_PreviewCurrentPresetPose(...)`
- `BattleTurnCamera::Evaluate_PreviewEnemyAttackPose(...)`

이 함수들은 외부에서 실제로 쓰이지 않았고,
이미 shared helper들이 존재하는 구조에서 우회 계층 역할만 하고 있었습니다.

삭제 기준은 간단합니다.

- 외부 사용이 없고
- 기존 기능을 더 직접적인 helper가 이미 제공하고 있고
- 오히려 책임만 헷갈리게 만든다

면 제거 후보입니다.

---

## 5. 검증은 어떻게 했나

이번 작업은 아래 순서로 검증했습니다.

### 5-1. 정적 검증

- `WIDE` 수치가 한 군데(`BattleTurnCameraPoseUtils.h`)로 모였는지 검색
- `Evaluate_ShotRequestPose(...)` 안에 `Apply_LookAt(...)` 같은 직접 적용이 남아 있지 않은지 확인
- 제거한 preview evaluator API 참조가 남아 있지 않은지 확인

### 5-2. 빌드 검증

실제로 아래 두 프로젝트 빌드를 확인했습니다.

- `Client\Default\Client.vcxproj` (`Debug|x64`)
- `MainEditor\Default\MainEditor.vcxproj` (`Debug|x64`)

중간에 컴파일 오류도 한 번 잡혔습니다.

예를 들면:

- event 타입을 header로 새어나가게 만들었던 선언 문제
- `const BattleCameraClip*`와 `BattleCameraClip*` 변환 문제

이런 오류를 수정한 뒤 다시 빌드해서 최종 통과시켰습니다.

이 포인트는 학습적으로 중요합니다.

> 리팩터링은 "코드가 예뻐 보이는 상태"가 아니라 "빌드와 호출 관계까지 검증된 상태"까지 포함합니다.

---

## 6. 이번 리팩터링을 포트폴리오에서 어떻게 설명하면 좋은가

포트폴리오 설명 예시는 아래처럼 잡을 수 있습니다.

### 한 줄 요약

Battle camera runtime/editor parity를 위해 shared pose evaluator를 도입하고,
mode별 asset resolution과 owner responsibility를 분리한 리팩터링을 수행했다.

### 조금 더 자세한 설명

- 중복된 `WIDE` camera calculation을 shared utility로 통합
- runtime evaluator와 transform apply responsibility를 분리
- event-to-request mapping과 mode별 preset/clip fallback 정책을 helper로 분리
- preview와 runtime이 같은 camera math를 사용하도록 맞춰 authoring reliability 개선
- `Client`, `MainEditor` 빌드 검증으로 변경 안전성 확인

### 면접/설명 포인트

면접에서는 "무엇을 지웠는가"보다 아래를 말하면 더 좋습니다.

- 왜 duplicated logic가 위험했는가
- 왜 preview/runtime parity가 중요한가
- 왜 evaluator와 apply를 분리했는가
- 왜 header가 아니라 `cpp` local helper가 적절했는가
- 왜 작은 wrapper가 항상 좋은 추상화는 아닌가

---

## 7. 혼자 복습할 때 다시 보면 좋은 코드 순서

아래 순서대로 보면 이해가 쉽습니다.

1. `Client/Public/BattleTurnCameraPoseUtils.h`
   `Evaluate_BattleTurnWidePose`, `Evaluate_BattleTurnPresetPose`, `Evaluate_BattleTurnEnemyAttackPose`

2. `Client/Private/BattleTurnCamera.cpp`
   `Evaluate_WidePose`, `Evaluate_ShotRequestPose`, `Resolve_ShotAssets`, `Apply_ShotRequest`

3. `MainEditor/Private/BattleCameraPreview_View.cpp`
   `Apply_BattleCameraPreviewPose`, `Evaluate_PreviewModePose`

이 순서가 좋은 이유는:

- 먼저 "공용 계산"
- 그다음 "runtime orchestration"
- 마지막으로 "editor reuse"

흐름으로 읽을 수 있기 때문입니다.

---

## 8. 직접 다시 해보는 연습 문제

### 연습 1

`WIDE` helper에 side offset이나 height tuning 옵션을 추가하고 싶다고 가정해보세요.

질문:

- 그 값은 helper 인자로 넣을까?
- `BattleCameraPreset` 쪽으로 넣을까?
- runtime과 preview 둘 다 어떤 식으로 영향을 받을까?

### 연습 2

`PLAYERFOCUS`와 `ENEMYFOCUS`가 사실상 같은 evaluator를 쓰고 있다면,
"정말로 별도 mode가 필요한가?"를 코드 구조 관점에서 생각해보세요.

### 연습 3

`Resolve_ShotAssets(...)`가 지금보다 더 커진다면,
어느 시점에서 별도 policy object나 table-driven 구조를 고려해야 할지 스스로 기준을 세워보세요.

### 연습 4

이번에 제거한 1-line wrapper들을 다시 살리고 싶다면,
어떤 조건이 만족될 때만 그 wrapper가 가치가 생기는지 생각해보세요.

예:

- mode별 telemetry를 넣는다
- mode별 pre/post hook이 생긴다
- mode별 진입 정책이 실제로 달라진다

그 전까지는 wrapper가 아니라 noise일 가능성이 높습니다.

---

## 9. 이번 작업에서 꼭 기억하면 좋은 리팩터링 원칙

마지막으로, 이번 작업을 통해 가져가면 좋은 원칙들을 정리하면:

1. 중복 제거는 "줄 수 줄이기"보다 "진실원이 한 곳인지"가 더 중요하다.
2. evaluator와 apply는 섞지 않는 편이 디버깅과 재사용에 유리하다.
3. runtime과 preview가 다르면 authoring tool 신뢰도가 무너진다.
4. header는 계약만 담고, 구현 detail은 가능하면 `cpp`에 숨긴다.
5. 작은 wrapper가 항상 좋은 추상화는 아니다.
6. 리팩터링은 마지막에 반드시 빌드/검증까지 가야 끝이다.

---

## 10. 짧은 회고

이번 작업은 "기능 추가"보다는 "구조 정리"에 가까운 리팩터링이었습니다.
하지만 실제로는 이런 작업이 장기적으로 더 중요합니다.

이유는:

- 새 기능을 넣을 때 덜 무섭고
- preview/runtime mismatch를 덜 만들고
- 나중에 내가 다시 봐도 이해하기 쉬워지기 때문입니다

당장 빠르게 구현하는 것도 중요하지만,
나중에 **내가 다시 설명할 수 있는 구조**로 만드는 것이 더 강한 실력입니다.

이 문서를 다시 볼 때는 "코드를 외우기"보다

- 왜 helper를 뺐는지
- 왜 어떤 것은 남기고 어떤 것은 지웠는지
- 왜 owner 경계를 나눴는지

를 중심으로 복기해보면 훨씬 오래 남습니다.
