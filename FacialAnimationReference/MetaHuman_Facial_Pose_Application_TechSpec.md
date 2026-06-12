# MetaHuman Facial Pose 적용 방안 기술서

## 목적

FModel로 추출한 MetaHuman facial morph 데이터를 엔진의 `.morph` 파일로 생성한 뒤, ModelEditor에서 실제 표정 preset을 선택해 morph weight를 적용하는 테스트 파이프라인을 구성한다.

현재 `.morph` 생성은 `MaelleHead.model + Maelle_v5_FaceMesh.glb` 기준으로 성공했다.

```text
targets=753
meshes=4
deltas=459503
```

다음 단계는 "어떤 weight 조합을 넣어야 특정 표정이 되는가"를 확인하고, ModelEditor에서 선택 적용할 수 있게 만드는 것이다.

## 참조 파일

아래 파일들을 D 드라이브 export 경로에서 현재 폴더로 복사했다.

```text
FacialAnimationReference/
  mh_arkit_mapping_pose.json
  mh_arkit_mapping_anim.json
  Expression_Sadness.json
  MetaHuman_Facial_Pose_Application_TechSpec.md
```

원본 경로:

```text
D:\Clair Obscur Expedition 33\Exports\Sandfall\Content\MetaHumans\Common\Common\Mocap\mh_arkit_mapping_pose.json
D:\Clair Obscur Expedition 33\Exports\Sandfall\Content\MetaHumans\Common\Common\Mocap\mh_arkit_mapping_anim.json
D:\Clair Obscur Expedition 33\Exports\Sandfall\Content\MetaHumans\Common\Face\Expression_Sadness.json
```

## 파일별 역할

### mh_arkit_mapping_pose.json

가장 중요한 적용 기준 파일이다.

이 파일은 `PoseAsset`이며, ARKit 표정 이름과 실제 MetaHuman curve weight 묶음을 가지고 있다.

확인된 구조:

```text
Properties.PoseContainer.PoseFNames
  - ARKit pose 이름 배열
  - 예: EyeBlinkLeft, JawOpen, MouthSmileLeft

Properties.PoseContainer.Poses
  - PoseFNames와 같은 index로 대응
  - 각 Pose 안에 CurveData 배열 존재

Properties.PoseContainer.Curves
  - CurveData index에 대응되는 curve 이름 배열
```

확인된 개수:

```text
PoseFNames: 66
Poses: 66
Curves: 1172
```

즉 적용 방식은 다음과 같다.

```text
poseIndex = PoseFNames에서 선택 pose 이름 검색
curveWeights = Poses[poseIndex].CurveData

for i in CurveData:
  curveName = Curves[i].CurveName
  weight = CurveData[i]
  if curveName exists in MorphData.targets:
    morphWeight[targetIndex] = weight
```

### mh_arkit_mapping_anim.json

`AnimSequence` 형식의 mapping animation이다.

확인된 정보:

```text
CompressedCurveNames: 1172
SequenceLength: 2.7083333
FrameRate: 24
```

`mh_arkit_mapping_pose.json`와 같은 curve 이름 체계를 가진다. 1차 구현에서는 pose 적용 테스트가 목적이므로 직접 재생 대상으로 쓰기보다는 curve name 검증/참조용으로 둔다.

### Expression_Sadness.json

`AnimSequence` 형식의 표정 샘플이다.

확인된 정보:

```text
Name: Expression_Sadness
SequenceLength: 0.033333335
CompressedCurveNames: 1089
```

단일 sadness 표정 샘플로 보인다. 1차 구현에서는 필수 입력이 아니며, pose preset 적용이 성공한 뒤 "표정 anim json을 pose/curve preset으로 변환할 수 있는가"를 검증할 때 사용한다.

## 실제 weight 예시

`mh_arkit_mapping_pose.json`에서 확인한 sample이다.

### JawOpen

```text
head_lod0_mesh__jaw_open = 1.0
```

### EyeBlinkLeft

```text
head_lod0_mesh__eye_blink_L = 0.88081807
head_lod0_mesh__eye_lookDown_L = -0.06629684
cartilage_lod0_mesh__eye_blink_L = 0.88081807
cartilage_lod0_mesh__eye_lookDown_L = -0.06629684
```

### MouthSmileLeft

```text
head_lod0_mesh__mouth_cornerPull_left = 1.0
```

### BrowDownLeft

```text
head_lod0_mesh__brow_down_L = 1.0
head_lod0_mesh__brow_lateral_L = 0.031404663
head_lod0_mesh__Bdown_Blateral__browLower_L = 0.031404663
```

## 적용 대상 curve 필터링

Pose file에는 morph target만 있는 것이 아니다.

섞여 있는 curve 유형:

```text
CTRL_expressions_*
head_cm*_color_*
head_lod0_mesh__*
cartilage_lod0_mesh__*
teeth_lod0_mesh__*
eyeLeft_lod0_mesh__*
eyeRight_lod0_mesh__*
```

현재 엔진 `.morph`에 적용 가능한 것은 `MorphData.targets`에 존재하는 이름뿐이다.

따라서 1차 구현에서는 다음 규칙을 사용한다.

```text
CurveName이 MorphData.targets에 있으면 적용
없으면 무시
```

무시되는 대표 curve:

```text
CTRL_expressions_*
head_cm*_color_*
```

이들은 ControlRig 또는 animated material mask 계열일 가능성이 높다. 현재 morph-only 테스트에서는 적용하지 않는다.

## ModelEditor 구현안

### UI

`Model Test Panel`에 morph pose 테스트 섹션을 추가하거나, 별도 `Morph Test Panel`을 만든다.

권장 UI:

```text
Morph Pose Test
  Pose Mapping File [Browse]
  Loaded: poses=66, curves=1172

  Pose Filter [input]
  Pose Combo/List
    Default
    EyeBlinkLeft
    EyeBlinkRight
    JawOpen
    MouthSmileLeft
    ...

  Weight [0.0 - 1.0]
  Apply Pose
  Reset Morph

  Matched Curves: N
  Missing Curves: N
```

### 데이터 구조

```cpp
struct FacialPoseCurve
{
    string curveName;
    float weight{};
};

struct FacialPosePreset
{
    string poseName;
    vector<FacialPoseCurve> curves;
};

struct FacialPoseMapping
{
    vector<FacialPosePreset> poses;
};
```

### 파서

신규 parser를 별도 파일로 분리하는 것이 좋다.

```text
ModelEditor/Public/FacialPoseMappingParser.h
ModelEditor/Private/FacialPoseMappingParser.cpp
```

역할:

```text
Load_PoseMapping(path)
  root[0].Properties.PoseContainer 읽기
  PoseFNames 배열 읽기
  Curves[].CurveName 배열 읽기
  Poses[].CurveData 배열 읽기
  PoseFNames[i] + Poses[i].CurveData를 FacialPosePreset으로 변환
```

주의:

```text
PoseFNames.size == Poses.size 이어야 함
Curves.size == CurveData.size 이어야 함
```

### 적용 API

현재 morph runtime에 weight를 넣는 API가 필요하다.

권장 최소 API:

```cpp
// ModelDeform
const MorphData* Get_MorphData() const;
bool Has_MorphData() const;
void Reset_MorphWeights();
void Set_MorphWeight(uint32 targetIndex, float weight);
void Set_MorphWeights(const vector<float>& weights);
const vector<float>& Get_MorphWeights() const;

// ModelCom forwarding
const MorphData* Get_MorphData() const;
void Reset_MorphWeights();
void Set_MorphWeight(uint32 targetIndex, float weight);
void Set_MorphWeights(const vector<float>& weights);
```

`AnimComputeRuntime` 쪽은 weight 변경 후 `_morphDirty = true`가 되어야 한다. 현재 구조상 weight 변경 이벤트를 어디서 dirty 처리할지 확인이 필요하다.

## 적용 알고리즘

```text
1. ModelEditor에서 MaelleHead.model 로드
2. 같은 폴더의 MaelleHead.morph 자동 로드
3. mh_arkit_mapping_pose.json 로드
4. 선택 Pose의 curve list를 가져옴
5. morphData.targets 이름 -> targetIndex lookup map 생성
6. weights 배열을 morphData.targets.size 만큼 0으로 초기화
7. 각 curve에 대해:
     if curve.curveName exists in lookup:
       weights[targetIndex] = curve.weight * uiWeight
8. ModelCom.Set_MorphWeights(weights)
9. compute morph dirty 처리
10. viewport에서 표정 변화 확인
```

## 검증 순서

1차 검증 pose:

```text
JawOpen
EyeBlinkLeft
EyeBlinkRight
BrowDownLeft
MouthSmileLeft
MouthSmileRight
```

권장 확인:

```text
JawOpen: 입/턱이 열리는지
EyeBlinkLeft: 왼쪽 눈꺼풀/카트리지가 같이 움직이는지
BrowDownLeft: 왼쪽 눈썹이 내려가는지
MouthSmileLeft: 왼쪽 입꼬리가 올라가는지
```

문제가 생겼을 때 판단:

```text
움직임이 없음:
  - .morph 로드 여부 확인
  - curveName과 morph target 이름 매칭 개수 확인
  - morph dirty 처리 확인

반대로 움직임:
  - GLB delta 축 변환 확인

너무 과하게 움직임:
  - delta scale 확인
  - uiWeight를 0.2~0.5로 낮춰 테스트

일부 부위만 움직임:
  - 해당 pose의 missing curve 목록 확인
  - cartilage/teeth/eye target이 .morph에 포함됐는지 확인
```

## 구현 우선순위

1. `FacialPoseMappingParser` 추가
2. ModelEditor UI에 pose mapping browse/load 추가
3. MorphData target name lookup 생성
4. 선택 pose -> weight 배열 변환
5. `ModelCom`/`ModelDeform` weight set API 추가
6. Runtime dirty 처리 연결
7. JawOpen/EyeBlinkLeft/MouthSmileLeft 테스트
8. `Expression_Sadness.json` 적용 가능성 검토

## 결론

현재 표정 적용에 필요한 실제 weight 조합 데이터는 `mh_arkit_mapping_pose.json`에 있다.

1차 적용은 `PoseFNames` 기반 ARKit preset을 선택하고, 해당 pose의 `CurveData` 중 `.morph` target 이름과 일치하는 curve만 weight로 넣는 방식이 가장 안전하다.

이 방식이 통과하면 이후에는 실제 facial animation curve track을 읽어 frame별 weight로 재생하는 단계로 확장할 수 있다.
