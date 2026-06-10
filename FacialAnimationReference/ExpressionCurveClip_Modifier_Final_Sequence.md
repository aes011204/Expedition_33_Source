# ExpressionCurveClip + Modifier 최종 구조 및 구현 시퀀스

## 1. 최종 방향

현재 엔진 구조에서는 facial/morph 적용을 별도 예외 시스템으로 만들기보다 기존 `AnimationModifier` 평가 흐름에 태우는 것이 가장 자연스럽다.

다만 `ExpressionModifier`가 `ModelDeform::Reset_MorphWeights()` 또는 `Set_MorphWeight()`를 직접 호출해서 최종 weight를 덮어쓰는 구조는 장기적으로 문제가 있다. 여러 modifier가 동시에 동작하면 서로의 결과를 지울 수 있기 때문이다.

최종 구조는 다음처럼 잡는다.

```text
MorphData
  - vertex delta 리소스
  - .model에 붙는 shape library
  - ModelDeform이 보관

ExpressionCurveClip
  - JawOpen, EyeBlinkLeft 같은 pose/curve 값 리소스
  - JSON은 import source이고 runtime data가 아님

ExpressionModifier
  - ExpressionCurveClip을 평가
  - curve binding을 통해 morph weight request 생성
  - ModelDeform accumulator에 request 누적

ModelDeform
  - MorphData 보관
  - 최종 MorphWeights 보관
  - 여러 modifier의 weight request 합성
  - dirty flag 관리

AnimComputeRuntime
  - ModelDeform dirty 감지
  - morph compute shader 실행
  - morph 적용 vertex를 skinning source로 사용
```

요약:

```text
Morph = 변형 데이터
Clip = 시간/포즈별 제어 데이터
Modifier = Clip을 평가해서 weight request를 만드는 실행 레이어
ModelDeform = weight request를 합성하고 최종 weight를 보관
Compute = 최종 vertex 변형 실행
```

## 2. 현재 엔진 구조와 맞는 이유

`AnimComputeRuntime::Update_Frame()` 흐름은 이미 modifier 기반 적용에 맞다.

```cpp
_model->Step_SampleAnimationPose(context.modelContext);
_model->Evaluate_AnimationModifiers(timeDelta);
_model->Step_RebuildCombinedBones();
```

그 뒤 morph dirty를 확인한다.

```cpp
if (_model->Has_MorphData())
{
    if (_model->Consume_MorphDirty())
    {
        _morphDirty = true;
        _skinningDirty = true;
    }
}
```

따라서 `ExpressionModifier`가 compute shader를 직접 호출할 필요는 없다.

```text
ExpressionModifier
  -> ModelCom.Add_MorphWeightRequest()
  -> ModelDeform accumulator
  -> ModelDeform dirty
  -> AnimComputeRuntime.Prepare_MorphDeform()
  -> Morph compute dispatch
```

이 구조가 좋은 이유:

```text
Modifier는 gameplay/animation layer
ModelDeform은 deform state layer
AnimComputeRuntime은 GPU execution layer
```

책임이 분리된다.

## 3. 리소스 구분

### .morph

이미 구현된 morph geometry 리소스다.

역할:

```text
target name list
mesh별 vertex delta
mesh별 vertex range
```

예:

```text
MaelleHead.morph
targets=753
meshes=4
deltas=459503
```

관리 주체:

```text
ModelDeform
```

### .exprclip

새로 정의할 expression curve 리소스다.

역할:

```text
pose/clip 이름
curve name
curve weight
binding 결과
```

권장 확장자:

```text
.exprclip
```

이름 이유:

```text
facial 전용으로 고정하지 않음
curve 기반 expression clip이라는 의미가 명확함
morph 외 material/control curve 확장 가능
```

## 4. 데이터 타입 설계

### Curve Binding Type

```cpp
enum class ExpressionCurveBindingType : uint8
{
    Unknown,
    MorphTarget,
    MaterialParameter,
    ControlRig,
};
```

1차 구현에서는 `MorphTarget`만 실제 적용한다.

나머지는 버리지 않고 보존한다.

```text
CTRL_expressions_*     -> ControlRig 후보
head_cm*_color_*       -> MaterialParameter 후보
head_lod0_mesh__*      -> MorphTarget 후보
cartilage_lod0_mesh__* -> MorphTarget 후보
teeth_lod0_mesh__*     -> MorphTarget 후보
eyeLeft_lod0_mesh__*   -> MorphTarget 후보
eyeRight_lod0_mesh__*  -> MorphTarget 후보
```

### Runtime Curve

```cpp
struct ExpressionCurveKey
{
    string curveName;
    ExpressionCurveBindingType bindingType{};
    uint32 bindingIndex{};
    float value{};
};
```

`bindingIndex` 의미:

```text
MorphTarget       -> MorphData.targets index
MaterialParameter -> material parameter table index
ControlRig        -> control index
Unknown           -> invalid
```

### Pose Clip

```cpp
struct ExpressionPoseClip
{
    string name;
    vector<ExpressionCurveKey> curves;
};
```

### Clip Set

```cpp
struct ExpressionCurveClip
{
    string sourceName;
    vector<ExpressionPoseClip> poses;
};
```

1차 대상은 `mh_arkit_mapping_pose.json`이다.

```text
PoseFNames[i] -> ExpressionPoseClip.name
Poses[i].CurveData[j] + Curves[j].CurveName -> ExpressionCurveKey
```

## 5. Morph Weight Accumulator 설계

### 필요한 이유

Modifier가 직접 reset/set 하면 여러 modifier가 충돌한다.

나쁜 흐름:

```text
ExpressionModifier A: Reset -> Smile 적용
ExpressionModifier B: Reset -> Blink 적용
결과: Smile 사라짐
```

권장 흐름:

```text
Begin_MorphWeightAccumulation()
  Modifier A -> Smile request 추가
  Modifier B -> Blink request 추가
End_MorphWeightAccumulation()
  최종 weights 확정
  변경된 경우에만 dirty
```

### Blend Mode

```cpp
enum class MorphWeightBlendMode : uint8
{
    Replace,
    Additive,
    Max,
};
```

### Request

```cpp
struct MorphWeightRequest
{
    uint32 targetIndex{};
    float value{};
    MorphWeightBlendMode blendMode{};
    int32 priority{};
};
```

### ModelDeform API

```cpp
class ModelDeform
{
public:
    void Begin_MorphWeightAccumulation();
    void Add_MorphWeightRequest(
        uint32 targetIndex,
        float value,
        MorphWeightBlendMode mode = MorphWeightBlendMode::Additive,
        int32 priority = 0);
    void End_MorphWeightAccumulation();
};
```

1차 구현은 단순화한다.

```text
Begin:
  accumulation buffer를 0으로 초기화

Additive:
  accumulation[targetIndex] += value

End:
  -1~1 clamp
  이전 weights와 비교
  변경된 경우만 _weights 갱신 및 dirty true
```

이후 확장:

```text
Replace:
  priority가 높은 request가 최종 값 대체

Max:
  abs(value)가 큰 쪽 유지 또는 max(value) 유지
```

## 6. 음수 Weight 정책

MetaHuman pose data에는 음수 curve weight가 실제로 존재한다.

예:

```text
EyeBlinkLeft:
head_lod0_mesh__eye_lookDown_L = -0.06629684
```

따라서 `ModelDeform::Set_MorphWeight()` 또는 accumulator clamp 범위가 `0~1`이면 데이터가 손실된다.

권장:

```text
MorphWeight runtime range = -1.0 ~ 1.0
ModelEditor 단일 target slider = -1.0 ~ 1.0 선택 가능
pose UI weight multiplier = 0.0 ~ 1.0
```

즉 clip 내부 curve value는 음수를 허용하고, pose 전체 강도만 0~1로 조절한다.

## 7. Dirty 캐싱

Modifier가 매 frame 같은 값을 요청하면 morph compute가 불필요하게 계속 돌 수 있다.

`End_MorphWeightAccumulation()`에서 이전 weights와 새 weights를 비교한다.

```text
변경 없음:
  dirty false 유지

변경 있음:
  _weights 갱신
  dirty true
```

float 비교는 epsilon을 둔다.

```text
abs(prev - next) > 0.0001
```

이렇게 하면 expression pose가 고정된 상태에서 매 frame morph compute가 다시 dispatch되는 것을 줄일 수 있다.

## 8. .exprclip Binary 포맷 초안

```text
uint32 magic = 'EXPC'
uint32 version = 1

string sourceName
uint32 poseCount

for pose:
  string poseName
  uint32 curveCount

  for curve:
    string curveName
    uint8 bindingType
    uint32 bindingIndex
    float value
```

주의:

```text
.exprclip은 특정 .morph target index와 결합될 수 있음
.morph target 순서가 바뀌면 bindingIndex가 깨질 수 있음
```

따라서 `.exprclip`은 index-only asset이면 안 된다.

```text
curveName = authoritative key
bindingIndex = cached fast path
```

Load 시 정책:

```text
1. 저장된 bindingIndex가 유효한지 확인
2. MorphData.targets[bindingIndex] == curveName 이면 그대로 사용
3. 다르면 curveName으로 재resolve
4. 재resolve 실패 시 Unknown으로 downgrade
```

## 9. Import 흐름

JSON을 runtime에서 직접 읽지 않는다.

권장 흐름:

```text
mh_arkit_mapping_pose.json
MaelleHead.morph
  -> ExpressionCurveClipBuilder
  -> MaelleHead.exprclip
```

Builder 역할:

```text
1. pose json 로드
2. .morph 로드 또는 현재 ModelDeform의 MorphData 참조
3. MorphData.targets로 name -> targetIndex map 생성
4. pose json의 curve 순회
5. curveName이 morph target에 존재하면 MorphTarget binding
6. 아니면 Unknown/ControlRig/MaterialParameter 후보로 보존
7. .exprclip 저장
```

1차 저장 시에는 실제 적용 가능한 `MorphTarget` binding만 적용 대상으로 사용한다.

단, 확장성을 위해 원본 curve name과 unresolved curve도 보존한다.

## 10. ExpressionModifier 설계

`AnimationModifier`를 상속한다.

```cpp
class ExpressionModifier final : public AnimationModifier
{
public:
    struct Desc : AnimationModifierDesc
    {
        Shared<ExpressionCurveClip> clip;
    };

    void Evaluate_Modifier(ModelCom* model, float timeDelta) override;

    void Set_Pose(const string& poseName);
    void Set_PoseWeight(float weight);
    void Reset();

private:
    Shared<ExpressionCurveClip> _clip{};
    int32 _poseIndex{ -1 };
    float _poseWeight{ 1.f };
};
```

### Evaluate 방식

1차 구현은 pose preset 적용만 한다.

```cpp
void ExpressionModifier::Evaluate_Modifier(ModelCom* model, float dt)
{
    if (!Is_Active() || model == nullptr || _clip == nullptr || _poseIndex < 0)
        return;

    const ExpressionPoseClip& pose = _clip->poses[_poseIndex];

    for (const ExpressionCurveKey& curve : pose.curves)
    {
        if (curve.bindingType != ExpressionCurveBindingType::MorphTarget)
            continue;

        model->Add_MorphWeightRequest(
            curve.bindingIndex,
            curve.value * Get_StrengthAlpha() * _poseWeight,
            MorphWeightBlendMode::Additive);
    }
}
```

중요:

```text
ExpressionModifier가 직접 Reset_MorphWeights()를 호출하지 않는다.
```

Reset과 최종 합성은 `ModelDeform`의 accumulation phase에서 한 번만 수행한다.

## 11. ModelCom Modifier Evaluation Phase

현재:

```cpp
void ModelCom::Evaluate_AnimationModifiers(float timeDelta)
{
    for (auto& modifier : _modifiers)
        modifier->Evaluate_Modifier(this, timeDelta);
}
```

개선:

```cpp
void ModelCom::Evaluate_AnimationModifiers(float timeDelta)
{
    Begin_MorphWeightAccumulation();

    for (auto& modifier : _modifiers)
    {
        if (modifier && modifier->Is_Active())
            modifier->Evaluate_Modifier(this, timeDelta);
    }

    End_MorphWeightAccumulation();
}
```

주의:

```text
모든 AnimationModifier가 morph를 쓰는 것은 아니다.
하지만 Begin/End가 매 frame 호출되어도 previous weights와 동일하면 dirty를 올리지 않아야 한다.
```

## 12. 구현 파일 구성안

### Engine

```text
Engine/Public/ExpressionCurveClip.h
Engine/Private/ExpressionCurveClip.cpp

Engine/Public/ExpressionModifier.h
Engine/Private/ExpressionModifier.cpp
```

### ModelEditor

```text
ModelEditor/Public/ExpressionCurveClipBuilder.h
ModelEditor/Private/ExpressionCurveClipBuilder.cpp
```

초기에는 `ModelTestPanel`에 임시 loader를 넣을 수도 있지만, 추천은 바로 분리다.

이유:

```text
JSON parsing
curve matching
binary export
UI
```

이 네 가지가 섞이면 나중에 정리 비용이 커진다.

## 13. 최종 구현 시퀀스

### 0단계: 단일 morph target 테스트 유지

이미 `Model Test Panel`에 추가한 직접 target slider는 유지한다.

용도:

```text
.morph 로드 검증
morph compute 검증
특정 target의 방향/scale 검증
```

이 단계가 깨지면 clip/modifier 구현을 해도 원인 추적이 어렵다.

### 1단계: ModelDeform weight range 수정

목표:

```text
Morph weight -1~1 허용
```

작업:

```text
Set_MorphWeight clamp 범위 0~1 -> -1~1
Model Test Panel slider 범위 -1~1로 확장
```

### 2단계: MorphWeight Accumulator 추가

목표:

```text
여러 modifier가 동시에 morph weight를 요청해도 서로 덮어쓰지 않게 한다.
```

작업:

```text
Begin_MorphWeightAccumulation
Add_MorphWeightRequest
End_MorphWeightAccumulation
dirty cache 비교
```

### 3단계: ModelCom evaluation phase 정리

목표:

```text
Modifier 평가 전후로 MorphWeight accumulation phase 보장
```

작업:

```text
Evaluate_AnimationModifiers에서 Begin/End 호출
inactive modifier skip
```

### 4단계: ExpressionCurveClip 타입 추가

목표:

```text
curve clip을 엔진 runtime 타입으로 정의
```

작업:

```text
ExpressionCurveBindingType 추가
ExpressionCurveKey 추가
ExpressionPoseClip 추가
ExpressionCurveClip 추가
```

### 5단계: Pose JSON importer 작성

목표:

```text
mh_arkit_mapping_pose.json -> ExpressionCurveClip 메모리 객체 변환
```

파싱 규칙:

```text
root[0].Properties.PoseContainer.PoseFNames
root[0].Properties.PoseContainer.Poses
root[0].Properties.PoseContainer.Curves
```

검증:

```text
poseCount = 66
curveCount = 1172
JawOpen에 head_lod0_mesh__jaw_open = 1.0 존재
EyeBlinkLeft에 eye_blink_L 계열 존재
```

### 6단계: Morph target binding resolver 작성

목표:

```text
curveName을 MorphData.targets index로 연결
```

작업:

```text
MorphData.targets로 unordered_map<string, uint32> 생성
curveName lookup
있으면 MorphTarget binding
없으면 이름 prefix로 후보 분류
```

검증:

```text
JawOpen matched >= 1
EyeBlinkLeft matched >= 2
BrowDownLeft matched >= 3
```

### 7단계: ExpressionCurveClip binary 저장/로드

목표:

```text
JSON import 결과를 .exprclip으로 저장
runtime에서는 .exprclip만 읽음
```

작업:

```text
Export_ExpressionCurveClip(path, clip)
Load_ExpressionCurveClip(path, outClip)
load 시 curveName/bindingIndex 검증 및 재resolve
```

저장 위치:

```text
MaelleHead.model
MaelleHead.morph
MaelleHead.exprclip
```

### 8단계: ExpressionModifier 추가

목표:

```text
AnimationModifier 흐름 안에서 expression pose를 적용
```

작업:

```text
ExpressionModifier : AnimationModifier
Set_Clip()
Set_Pose()
Set_PoseWeight()
Evaluate_Modifier()
```

적용:

```text
model->Add_MorphWeightRequest(targetIndex, value * alpha, Additive)
```

검증:

```text
JawOpen pose 선택 시 morph dirty 발생
AnimComputeRuntime morph dispatch
viewport에서 jaw open 확인
```

### 9단계: ModelEditor UI 연결

목표:

```text
에디터에서 .exprclip 로드 후 pose 선택 테스트
```

UI:

```text
Expression Clip [Browse]
Load
Pose Filter
Pose List
Pose Weight Slider
Apply/Enable Modifier
Reset

Matched curves
Unresolved curves
```

초기 UI는 `Model Test Panel`에 넣고, 안정화 후 별도 panel로 분리한다.

### 10단계: 자동 로드 정책 결정

선택지:

```text
A. .model sibling 자동 로드
   MaelleHead.exprclip 자동 탐색

B. Modifier 생성 시 명시 로드
   gameplay/editor에서 clip 지정
```

초기 추천:

```text
ModelEditor는 명시 로드
Runtime/gameplay는 필요 시 data table 또는 component setup에서 연결
```

`.morph`는 model에 강하게 결합되지만, `.exprclip`은 여러 캐릭터가 공유할 수도 있기 때문이다.

### 11단계: AnimSequence curve clip 확장

`Expression_Sadness.json` 같은 AnimSequence는 다음 단계다.

추가 타입:

```cpp
struct ExpressionCurveTrack
{
    string curveName;
    ExpressionCurveBindingType bindingType;
    uint32 bindingIndex;
    vector<float> samples;
};

struct ExpressionAnimClip
{
    string name;
    float duration;
    float frameRate;
    vector<ExpressionCurveTrack> tracks;
};
```

Modifier 확장:

```text
time 누적
frame interpolation
track sample
morph weight request 추가
```

이 단계는 pose preset 검증 후 진행한다.

## 14. 최종 런타임 시퀀스

```text
1. ModelCom Ready_Model
2. .model 로드
3. sibling .morph 로드
4. ModelDeform에 MorphData 등록
5. ExpressionCurveClip 로드
6. ExpressionModifier 생성
7. ModelCom.Add_AnimModifier(ExpressionModifier)
8. 매 frame:
   8.1 animation pose sample
   8.2 ModelDeform.Begin_MorphWeightAccumulation
   8.3 ExpressionModifier Evaluate
   8.4 ModelDeform.Add_MorphWeightRequest
   8.5 ModelDeform.End_MorphWeightAccumulation
   8.6 변경된 경우 morph weights dirty
   8.7 combined bone rebuild
   8.8 morph compute dispatch
   8.9 skinning compute dispatch
   8.10 render
```

## 15. 우선 테스트 Pose

```text
JawOpen
EyeBlinkLeft
EyeBlinkRight
BrowDownLeft
MouthSmileLeft
MouthSmileRight
```

확인 포인트:

```text
JawOpen:
  head_lod0_mesh__jaw_open = 1.0

EyeBlinkLeft:
  head_lod0_mesh__eye_blink_L = 0.88081807
  cartilage_lod0_mesh__eye_blink_L = 0.88081807

MouthSmileLeft:
  head_lod0_mesh__mouth_cornerPull_left = 1.0

BrowDownLeft:
  head_lod0_mesh__brow_down_L = 1.0
```

## 16. 남는 한계와 후속 확장

### Morph-only snapshot 검증 결과

ModelEditor에서 `Expression_Anger.curveclip`을 `MaelleHead.morph`에 직접 적용해 검증했다.

확인된 상태:

```text
CurveClip: Expression_Anger
curve count: 315
Maelle morph target exact match: 165
preview weight 1.0:
  max vertex displacement ~= 0.2108
  vertices over 1.0 displacement = 0
preview weight 5.0:
  max vertex displacement ~= 0.6056
  vertices over 1.0 displacement = 0
```

즉 `CurveClip -> MorphWeight` 경로 자체는 동작하지만, 해당 snapshot만으로는 눈에 띄는 facial expression을 만들기 어렵다.

검증을 위해 preview weight를 크게 열면 mesh가 찌그러지는 것은 확인된다. 이는 파이프라인이 완전히 죽어 있는 것이 아니라, morph-only로 과증폭하면 corrective delta가 의도 범위를 벗어나기 때문이다.

따라서 현재 방식의 의미는 다음으로 제한한다.

```text
가능:
  - curve name과 MorphData.target 매칭 검증
  - MorphData delta 방향/좌표계 검증
  - AnimComputeRuntime morph dispatch 경로 검증

불충분:
  - UE MetaHuman 원본 표정 재현
  - jaw / eye / brow 같은 큰 facial control 표현
  - corrective morph와 facial bone/control transform의 결합 결과 재현
```

특히 `jaw_open`, `brow_*`, `mouth_*` 같은 curve가 morph target에 존재하더라도, UE 쪽에서는 이것이 최종 표정 전체가 아니라 facial rig, joint, control, corrective morph가 결합된 결과 중 일부일 가능성이 높다.

결론:

```text
Morph-only CurveClip snapshot = 1차 검증/디버그 레이어
최종 facial expression = CurveClip + ExpressionModifier + MorphTarget + FacialBone/Control + Material curve
```

따라서 최종 구현 방향은 기존 TechSpec대로 `ExpressionCurveClip`을 단순 morph preset으로 보지 않고, 여러 binding type을 가진 expression clip으로 확장하는 것이다.

### Material / ControlRig curve

초기에는 무시해도 된다.

하지만 데이터는 보존한다.

```text
head_cm*_color_* -> material animated map 후보
CTRL_expressions_* -> control rig 후보
```

추후 material expression까지 붙이면 표정 품질이 올라갈 수 있다.

### Modifier 우선순위

나중에 blink, lip sync, emotion, gameplay expression이 동시에 들어오면 합성 정책이 필요하다.

예:

```text
base emotion: Additive priority 0
lip sync: Additive/Replace priority 5
blink: Max/Additive priority 10
cinematic override: Replace priority 100
```

초기에는 Additive만 사용해도 된다.

하지만 API에는 priority와 blendMode를 남겨두는 것이 좋다.

## 17. 최종 정리

가장 적절한 구조는 다음이다.

```text
MorphData
  shape delta resource

ExpressionCurveClip
  pose/curve value resource

ExpressionModifier
  animation modifier layer
  curve clip을 평가해서 ModelDeform accumulator에 request 추가

ModelDeform
  morph state holder
  request accumulator
  dirty flag

AnimComputeRuntime
  compute execution
```

이 구조는 현재 엔진의 `AnimationModifier`와 `AnimComputeRuntime` 설계에 맞고, 나중에 facial animation clip, lip sync, material curve, control rig curve까지 확장하기 쉽다.
