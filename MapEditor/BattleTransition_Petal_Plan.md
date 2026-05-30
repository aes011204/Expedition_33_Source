# 🌹 장미 꽃잎 배틀 트랜지션 (Petals Battle Transition) 구현 계획서 (수정판)

## 1. 개요 (Overview)
'Clair Obscur: Expedition 33'의 전투 진입 연출을 분석하여, 현재 엔진의 `ScreenFx_Manager` 파이프라인에 통합하기 위한 실행 계획입니다. 파티클 시스템(Niagara) 연동 시 발생하는 오버헤드를 완전히 제거하고, **단일 패스(Single-Pass) 쉐이더 조합만으로 흩날리는 꽃잎 효과와 화면 전환을 동시에 렌더링**하는 고도의 최적화 방식을 채택합니다.

**기본 전략 (Chained Pipeline)**:
기존에 완성된 `PS_Distortion`을 활용하여 화면을 왜곡(어안/줌)시키고, 그 결과를 이어받아 새로운 `PS_PetalWipeTransition` (7번 패스)에서 노이즈 텍스처를 활용해 꽃잎이 흩날리며 찢어지는 화면 전환을 완성합니다.

---

## 2. 리소스 확보 단계 (Resource Gathering)
단일 패스 쉐이더에서 경계면을 찢고 가짜 꽃잎(Fake Particle)을 흩뿌리기 위한 마스크 소스를 준비합니다.

*   [ ] **디졸브(Dissolve) 노이즈 텍스처**: `T_Random_49` 또는 `Good64x64TilingNoiseHighFreq` 추출.
    *   **목적**: 와이프(Wipe) 경계면이 거칠게 타들어가게 하고, 흩날리는 조각들의 위치를 결정하는 난수 역할.
    *   **저장 경로**: `Client/Bin/Resources/Textures/Transition/T_DissolveNoise.dds`
*   [ ] **꽃잎 마스크 텍스처**: `T_Paint1` 추출.
    *   **목적**: 단순히 찢어지는 것을 넘어, 떨어져 나가는 조각들이 꽃잎 형태(`T_Paint1`의 알파)를 띄도록 마스킹.
    *   **저장 경로**: `Client/Bin/Resources/Textures/Transition/T_PetalMask.dds`
*   [ ] **엔진 캐시 등록**: `ShaderCom::Bind_DrawFeatures` 혹은 전용 Texture Slot을 확장하여, ScreenFx가 실행될 때 위 노이즈/마스크 텍스처를 HLSL 레지스터(예: `t2`, `t3`)에 바인딩할 수 있도록 구조 마련.

---

## 3. HLSL 쉐이더 고도화 (The 7th Pass)
기존 원형 와이프를 보존한 채, 꽃잎 연출 전용 패스(`PS_PetalWipeTransition`)를 신규 7번 패스로 파서 구성합니다.

*   [ ] **신규 패스 추가 (`Shader_ScreenFx.hlsl`)**:
    *   `technique11 DefaultTechnique`의 마지막 줄에 7번 패스 `PS_PetalWipeTransition` 추가.
    *   C++ 쪽 `ScreenFx_Type` Enum에 `PetalWipe = 7` 항목 추가.
*   [ ] **Noise 기반 경계면 Dissolve 적용**:
    *   기존 원형 거리(`dist`)에 노이즈 텍스처(`T_DissolveNoise`)의 값을 곱하거나 더해, 원의 테두리가 완벽한 동그라미가 아닌 톱니바퀴처럼 불규칙하게 파이도록 수식 작성.
*   [ ] **Fake Petals (꽃잎 파편 생성)**:
    *   UV 공간(Screen Space)을 특정 격자로 나누고(Cellular Noise), 임계값을 넘는 셀에만 `T_PetalMask`를 샘플링합니다.
    *   시간(`g_transitionProgress` 또는 `g_wipeRadius`에 비례)에 따라 격자의 UV 좌표를 바깥쪽으로 밀어내어, 꽃잎 조각들이 화면 중앙에서 흩뿌려지는 듯한 시각적 착시(Optical Illusion) 부여.
    *   조각(꽃잎)이 생성된 픽셀은 장미색/황금색(`C8A546`) Color Tint를 입혀 리턴.

---

## 4. 왜곡(Distortion) 패스와의 체인 연동 (Chaining & C++)
기존 `PS_Distortion` 패스를 그대로 재사용하여, 렌즈 왜곡 효과와 꽃잎 와이프 효과를 동시에(Ping-Pong) 적용합니다.

*   [ ] **신규 C++ 인스턴스 생성**:
    *   `PetalWipe_ScreenFx.cpp/h` 클래스 생성. (내부 로직은 `BattleTransition_ScreenFx`와 유사하나, 노이즈 강도나 파편 스피드 관련 변수가 CBuffer에 추가됨).
*   [ ] **다중 이펙트 동시 재생 (`ClientScreenFxManager`)**:
    *   `Transition_To_BattleField()` 호출 시 다음 **2개의 이펙트를 매니저에 동시에 Push**합니다.
        1.  `Distortion_ScreenFx` (어안 렌즈처럼 화면 가운데로 빨려 들어가는 줌 효과)
        2.  `PetalWipe_ScreenFx` (노이즈 기반 꽃잎 트랜지션)
    *   엔진의 Ping-Pong 아키텍처 덕분에 1번의 왜곡 결과물(RT)을 2번 패스가 그대로 물려받아(마스크 처리), 원본 게임과 동일한 융합 연출이 완벽하게 작동합니다.
*   [ ] **상수 버퍼(CBuffer) 정리**:
    *   HLSL `ScreenFxBuffer`에 노이즈 타일링 배율, 꽃잎 파편 개수, 흩날리는 속도 등을 조절할 파라미터 추가 후 C++ 구조체 정렬 동기화.

---
**최종 파이프라인 흐름**: 
`캡처 (Snapshot)` ➡️ `Pass 5 (Distortion: 화면 렌즈 왜곡)` ➡️ `Pass 7 (PetalWipe: 왜곡된 화면 위에 노이즈 꽃잎 마스킹 및 전환)` ➡️ `최종 화면 (백버퍼)`
