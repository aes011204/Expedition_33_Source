# FacialClip Runtime Integration 정리

## 목적

MetaHuman 기반 hero head preview에서 임시 `CurveClip` 표정 처리를 제거하고, 원본 facial export 기반의 morph curve와 facial bone track을 같은 timeline으로 재생하기 위한 런타임 구조를 정리한다.

현재 적용 범위는 Character Status Panel / Character Skill Panel의 UI preview facial 재생이다. 전투 대사 lip-sync, cutscene facial, 월드 실시간 표정은 같은 구조를 재사용할 수 있지만 이 문서의 1차 범위는 아니다.

## 생성 파이프라인

입력 데이터는 `Client/Bin/Resources/Animations/Facial/Metahuman_Common/MicroMovements` 아래의 `A_MH_RandomMovement_GameMenu.json` + `.psa`와 각 hero head의 `.model.meta`, `.morph` 파일이다.

### 배치툴 사용 순서

처음부터 다시 생성할 때는 모델 에디터에서 수동으로 뭔가를 누르는 흐름이 아니다. 원본 추출 데이터와 현재 프로젝트 head binding 파일을 배치툴에 넣고, hero별 runtime clip을 다시 뽑는 방식이다.

1. 원본 facial animation 입력 파일이 있는지 확인한다.

```text
Client/Bin/Resources/Animations/Facial/Metahuman_Common/MicroMovements/A_MH_RandomMovement_GameMenu.json
Client/Bin/Resources/Animations/Facial/Metahuman_Common/MicroMovements/A_MH_RandomMovement_GameMenu.psa
```

2. 각 hero head binding 파일이 있는지 확인한다.

```text
Client/Bin/Resources/Models/Heros/Gustave/GustaveHead/GustaveHead.model.meta
Client/Bin/Resources/Models/Heros/Gustave/GustaveHead/GustaveHead.morph
Client/Bin/Resources/Models/Heros/Lune/LuneHead/LuneHead.model.meta
Client/Bin/Resources/Models/Heros/Lune/LuneHead/LuneHead.morph
Client/Bin/Resources/Models/Heros/Maelle/MaelleHead/MaelleHead.model.meta
Client/Bin/Resources/Models/Heros/Maelle/MaelleHead/MaelleHead.morph
```

3. repo root 기준으로 배치툴 폴더에 들어간다.

```bat
cd /d D:\GitDesktop\Expedition_158\BatchTool\Facial
```

4. 세 hero를 한 번에 생성한다.

```bat
BuildFacialClip_AllHeroes.bat
```

5. 실행이 끝나면 `Client/Bin/Resources/Animations/Facial/Metahuman_Common/MicroMovements` 아래에 hero별 `.facialclipbin`, `.facialclip.json`, `.facialclip.report.json`이 생겼는지 확인한다.

```powershell
Get-ChildItem D:\GitDesktop\Expedition_158\Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements -Filter A_MH_RandomMovement_GameMenu*.facialclip*
```

6. 먼저 `.report.json`을 본다. report에서 morph/bone 매칭 수가 크게 줄어 있으면 런타임을 보기 전에 `.model.meta` 또는 `.morph` binding이 잘못된 것이다.

```powershell
Get-Content -Encoding UTF8 Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu_Gustave.facialclip.report.json
Get-Content -Encoding UTF8 Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu_Lune.facialclip.report.json
Get-Content -Encoding UTF8 Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu_Maelle.facialclip.report.json
```

7. 게임 런타임에서는 별도 수동 등록이 필요 없다. `PW_Gustave`, `PW_Lune`, `PW_Maelle`의 `Get_GameMenuFacialClipPath()`가 각 hero용 `.facialclipbin`을 가리키고, preview 갱신 중 `Player_WorldBase::Update_GameMenuFacialPreview`가 lazy load한다.

배치툴을 다시 돌려야 하는 경우:

- 원본 `A_MH_RandomMovement_GameMenu.json` 또는 `.psa`를 새로 추출했을 때.
- hero head의 `.model.meta` 또는 `.morph`를 다시 만들었을 때.
- report의 matched morph/bone 수가 이상해서 binding을 고친 뒤 다시 검증할 때.

배치툴을 다시 돌릴 필요가 없는 경우:

- `FacialAnimationModifier.cpp`의 blink/eye/mouth 보정 상수만 바꿨을 때. 이 경우 산출물은 그대로 두고 코드 빌드만 다시 하면 된다.
- UI preview 호출 위치만 바꿨을 때.

### 산출물

생성 결과는 hero별로 나뉜다.

```text
A_MH_RandomMovement_GameMenu_Gustave.facialclip.json
A_MH_RandomMovement_GameMenu_Gustave.facialclip.report.json
A_MH_RandomMovement_GameMenu_Gustave.facialclipbin
A_MH_RandomMovement_GameMenu_Lune.facialclip.json
A_MH_RandomMovement_GameMenu_Lune.facialclip.report.json
A_MH_RandomMovement_GameMenu_Lune.facialclipbin
A_MH_RandomMovement_GameMenu_Maelle.facialclip.json
A_MH_RandomMovement_GameMenu_Maelle.facialclip.report.json
A_MH_RandomMovement_GameMenu_Maelle.facialclipbin
```

런타임은 `.facialclipbin`만 로드한다. `.facialclip.json`과 `.report.json`은 사람이 매칭 상태와 원본 값을 확인하기 위한 디버그 산출물이다.

`BuildFacialClip.bat`은 Gustave 기준 단일 생성/디버그용으로 남아 있다. 현재 게임에 실제로 쓰는 정식 흐름은 `BuildFacialClip_AllHeroes.bat`이다.

현재 report 기준값:

| Hero | Duration | Morph | Bone |
| --- | ---: | ---: | ---: |
| Gustave | 17.633333s | 182 / 315 | 779 / 779 |
| Lune | 17.633333s | 182 / 315 | 779 / 779 |
| Maelle | 17.633333s | 182 / 315 | 779 / 779 |

## 런타임 구조

`FacialClip`은 `.facialclipbin`을 읽어 duration, additive flag, morph track, bone track을 보관한다. Morph track은 curve name과 float keyframes, bone track은 bone name과 transform keyframes를 가진다.

`FacialAnimationModifier`는 `AnimationModifier` 계열로 구현되어 같은 `timeSec`에서 morph와 bone을 평가한다.

- Morph는 `ModelCom::Find_MorphTargetIndex`로 매칭되는 target만 적용한다.
- Bone은 `ModelCom::Get_BoneIndex`로 매칭되는 facial bone만 적용한다.
- Additive local clip은 현재 head model의 captured base local 위에 delta를 합성한다.
- 비활성화 시 적용했던 morph weight와 facial bone local pose를 base pose로 복구한다.

UI preview 적용은 `Player_WorldBase::Update_GameMenuFacialPreview`에서 처리한다. `UIPreviewManager`는 status / skill panel preview update 중 이 함수를 호출하고, 각 `PW_*` 클래스는 자기 hero용 `.facialclipbin` 경로만 override한다.

캐릭터별 runtime file:

```text
Gustave: Animations/Facial/Metahuman_Common/MicroMovements/A_MH_RandomMovement_GameMenu_Gustave.facialclipbin
Lune:    Animations/Facial/Metahuman_Common/MicroMovements/A_MH_RandomMovement_GameMenu_Lune.facialclipbin
Maelle:  Animations/Facial/Metahuman_Common/MicroMovements/A_MH_RandomMovement_GameMenu_Maelle.facialclipbin
```

## 현재 보정 정책

원본 PSA / morph 값을 그대로 적용하면 프로젝트 head와 좌표계/단위 차이 때문에 얼굴이 찌그러지거나, 눈동자가 과회전하거나, 입이 열리는 문제가 있었다. GameMenu preview에서는 아래 보정을 적용한다.

| 값 | 현재값 | 용도 |
| --- | ---: | --- |
| `kAdditiveTranslationScale` | `0.01f` | ActorX/UE raw translation을 엔진 단위에 맞게 축소 |
| `kEyeRotationScale` | `0.25f` | `FACIAL_L_Eye`, `FACIAL_R_Eye` 회전 과장을 감쇠 |
| `kBlinkMorphGamma` | `2.2f` | 낮은 blink 값을 더 약하게 눌러 중간 표정 방지 |
| `kBlinkMorphPeakScale` | `1.75f` | blink peak에서 morph 닫힘을 보강 |
| `kBlinkClosureStart` | `0.42f` | upper eyelid 보정 시작 blink 값 |
| `kBlinkClosureEnd` | `0.68f` | upper eyelid 보정 최대 blink 값 |
| `kUpperEyelidClosureOffsetY` | `-0.085f` | blink peak 때 upper eyelid에 추가로 주는 닫힘 offset |

중요한 해석:

- PSA additive scale key가 `[0, 0, 0]`으로 들어오는 경우가 많다. 이 값을 그대로 scale matrix로 쓰면 얼굴이 0배 scale로 찌그러진다. Additive scale은 `Vec3::One + key.scale`로 해석한다.
- `Mouth`, `Lip`, `Jaw`, `Chin`, `Nasolabial` 계열 track은 GameMenu preview에서 입 벌어짐을 만들기 때문에 `suppressMouthArea = true`일 때 제외한다.
- 눈동자는 원본 rotation delta가 프로젝트 preview에서 과하게 보이므로 `FACIAL_L_Eye`, `FACIAL_R_Eye`만 감쇠한다.
- Blink는 낮은 값에서는 덜 내려오고, peak에서만 확 닫히도록 gamma shaping과 upper eyelid closure offset을 함께 쓴다.

## 후속 튜닝 가이드

눈이 아직 덜 감기면 `kUpperEyelidClosureOffsetY`를 더 음수로 내린다. 예: `-0.095f`, `-0.10f`.

눈을 감지 않을 때 게슴츠레하거나 째려보면 `kBlinkClosureStart` / `kBlinkClosureEnd`를 올리거나, `kUpperEyelidClosureOffsetY`의 절댓값을 낮춘다.

눈동자가 너무 많이 움직이면 `kEyeRotationScale`을 낮춘다. 너무 죽으면 `0.30f` 근처로 올린다.

입이 다시 움직이면 `Is_MouthAreaTrack`의 제외 대상 문자열에 누락된 mouth area bone/morph 이름이 있는지 확인한다.

얼굴이 찌그러지면 additive scale 해석과 translation scale 적용 순서를 먼저 확인한다. 특히 closure offset은 translation unit scale 이후 최종 local offset으로 더해야 한다.

## 검증 메모

초기 통합 후 `Debug|x64` 기준으로 `Engine`, `Client`, `MainEditor`, 전체 solution 빌드를 통과했다. 이후 세부 blink tuning은 사용자의 요청에 따라 빌드를 추가로 돌리지 않고 `git diff --check`만 확인했다.

문서와 코드가 어긋나는지 확인할 때는 다음을 본다.

```powershell
rg -n "kAdditiveTranslationScale|kEyeRotationScale|kBlinkMorphGamma|kBlinkMorphPeakScale|kBlinkClosureStart|kBlinkClosureEnd|kUpperEyelidClosureOffsetY" Engine\Private\FacialAnimationModifier.cpp
rg -n "Get_GameMenuFacialClipPath|suppressMouthArea" Client\Public Client\Private
Get-ChildItem Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements -Filter A_MH_RandomMovement_GameMenu*.facialclip*
```
