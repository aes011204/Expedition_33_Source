#pragma once

#include "Engine_Typedef.h"

NS_BEGIN(Engine)

inline constexpr uint32 kEffectDistributionCurveMaxKeys{ 8 };

// point particle 검증 경로에서 사용할 instanced draw 제출 방식.
enum class PointParticleDrawMode : uint8
{
    Direct,                      // 기존 DrawInstanced 경로로 제출한다.
    DrawInstancedIndirect,       // CPU가 작성한 args buffer로 DrawInstancedIndirect를 호출한다.
    DrawIndexedInstancedIndirect // CPU가 작성한 args buffer로 DrawIndexedInstancedIndirect를 호출한다.
};

// SubUV frame 선택/재생 기본 방식을 결정한다.
enum class SubUVFramePlaybackMode : uint8
{
    FixedFrame,
    LifeProgress,
    FramesPerSecond,
    RandomFrame,
};

// GPU instance buffer에 저장되는 per-instance world/lifetime/color payload.
struct ParticleInstanceVertex
{
    Vec4 right{};                          // 인스턴스 월드 행렬의 right 행.
    Vec4 up{};                             // 인스턴스 월드 행렬의 up 행.
    Vec4 look{};                           // compute sprite에서는 xyz를 velocity alignment hint로도 사용한다.
    Vec4 translation{};                    // 인스턴스 월드 행렬의 위치 행.
    Vec2 lifeTime{};                       // x: 최대 생존 시간, y: 누적 생존 시간.
    Vec4 startColor{ 1.f, 1.f, 1.f, 1.f }; // particle 생성 시점의 색상/알파.
    Vec4 endColor{ 1.f, 1.f, 1.f, 1.f };   // particle 수명 종료 시점의 색상/알파.
    Vec4 subUVRect{ 0.f, 0.f, 1.f, 1.f };  // x/y는 uv 시작점, z/w는 uv 끝점이다.
    Vec2 spriteTiltDegrees{};              // x/y: roll 적용 이후 card local X/Y 기준 tilt degree다.
    Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f }; // Core Color RGB Uniform+ParticleLife 생성 시점 샘플이다.
};

// Particle shader가 사용하는 vertex + instance input layout.
struct ParticleInstanceLayout
{
    static constexpr uint32 numElements = 11;

    // WORLD 0~3은 semantic name WORLD + semantic index 0~3으로 표현한다.
    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 104, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 1, 120, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 128, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

struct TrailInstanceVertex
{
    Vec4 right{};         // segment 진행 방향과 길이를 담는 instance row다.
    Vec4 up{};            // base-tip 폭 방향과 길이를 담는 instance row다.
    Vec4 look{};          // v0에서는 camera-facing 보정 없이 예비 축으로 둔다.
    Vec4 translation{};   // segment 중심 world position이다.
    Vec2 lifeTime{};      // x: segmentLifetime, y: age.
    Vec4 startColor{};    // segment 시작 색상이다.
    Vec4 endColor{};      // segment 끝 색상이다.
    Vec4 subUVRect{};     // segment별 trail UV 영역이다.
    Vec4 segmentParams{}; // x: age 진행률, y: width 배율, z: alpha 배율, w: reserved.
    Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f };
};

// Trail shader가 사용하는 vertex + instance input layout.
struct TrailInstanceLayout
{
    static constexpr uint32 numElements = 11;

    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 104, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 120, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 136, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

struct EffectMeshInstanceLayout
{
    static constexpr uint32 numElements = 12;

    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "INST_WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_LIFE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 80, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_CORE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 96, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

// Source-history ribbon shader가 사용하는 vertex + instance input layout.
// ComputeSourceHistoryRibbonEmitter::RibbonInstanceVertex와 byte layout을 맞춘다.
struct RibbonInstanceLayout
{
    static constexpr uint32 numElements = 11; // ComputeSourceHistoryRibbonEmitter::RibbonInstanceVertex와 byte layout을 맞춘다.

    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 104, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 120, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 136, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

// Generated beam shader가 사용하는 vertex + instance input layout.
// ComputeGeneratedBeamEmitter::BeamInstanceVertex와 byte layout을 맞춘다.
struct BeamInstanceLayout
{
    static constexpr uint32 numElements = 11;

    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 104, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 120, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 136, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

// Source-history sprite trail shader가 사용하는 vertex + instance input layout.
// ComputeSourceHistorySpriteTrailEmitter::StampInstanceVertex와 byte layout을 맞춘다.
struct SourceHistorySpriteTrailInstanceLayout
{
    static constexpr uint32 numElements = 9;

    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 40, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 56, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 104, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

// SubUVFrameOverLife 모듈의 최소 preview runtime desc다.
struct PointParticleSubUVFrameOverLifeDesc
{
    bool enabled{ false };                    // SubUV frame over life 모듈이 실제로 켜져 있는지 나타낸다.
    uint32 startFrame{ 0 };                   // FixedFrame에서는 선택 frame, 재생 모드에서는 시작 frame이다.
    uint32 endFrame{ 0 };                     // 재생 모드에서 종료 frame이다.
    bool loop{ false };                       // FPS 재생에서 frame range를 반복할지 나타낸다.
    SubUVFramePlaybackMode playbackMode{
        SubUVFramePlaybackMode::FixedFrame }; // 고정 선택, 수명 맞춤, FPS 재생, 랜덤 선택 중 하나를 결정한다.
    float framesPerSecond{ 12.f };            // FPS 재생 모드에서 초당 넘길 frame 수다.
    bool randomStartPhase{ false };           // FPS 재생에서 particle/stamp/strip별 시작 phase를 다르게 할지 나타낸다.
    uint32 frameCurveKeyCount{ 1 };           // 후속 Curve Editor 이관 후보인 compact frame curve key 수다.
    Vec4 frameCurveTimes{};                   // 후속 Curve Editor용 key time 0..1 값이다.
    Vec4 frameCurveTimesBlock1{};             // compact curve key 4..7 time 값이다.
    Vec4 frameCurveValues{};                  // 후속 Curve Editor용 frame index 값이다.
    Vec4 frameCurveValuesBlock1{};            // compact curve key 4..7 frame index 값이다.
};

// Float ConstantCurve distribution을 runtime에서 평가하기 위한 compact payload다.
struct PointParticleFloatCurveRuntimeDesc
{
    bool enabled{ false };                     // true면 sampling event phase 기준 curve를 평가한다.
    uint32 curveKeyCount{ 2 };                 // compact curve key 수다.
    Vec4 curveKeyTimes{ 0.f, 1.f, 0.f, 0.f };  // key time 0..1 값이다.
    Vec4 curveKeyTimesBlock1{};                // key 4..7 time 값이다.
    Vec4 curveKeyValues{ 1.f, 1.f, 0.f, 0.f }; // key별 float 값이다.
    Vec4 curveKeyValuesBlock1{};               // key 4..7 float 값이다.
    Vec4 curveKeyArriveTangents{};             // key별 arrive tangent 값이다.
    Vec4 curveKeyArriveTangentsBlock1{};       // key 4..7 arrive tangent 값이다.
    Vec4 curveKeyLeaveTangents{};              // key별 leave tangent 값이다.
    Vec4 curveKeyLeaveTangentsBlock1{};        // key 4..7 leave tangent 값이다.
    Vec4 curveKeyModes{ 1.f, 1.f, 0.f, 0.f };  // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 curveKeyModesBlock1{};                // key 4..7 interpolation mode 값이다.
};

enum class PointParticleSizeByLifeAxisLock : uint8
{
    None,
    X,
    Y,
};

// SizeByLife 모듈의 최소 preview runtime desc다.
struct PointParticleSizeByLifeDesc
{
    bool enabled{ false };                      // Size By Life 모듈이 실제로 켜져 있는지 나타낸다.
    float multiplyXStart{ 1.f };                // legacy endpoint fallback: 수명 시작 시 X축 배율이다.
    float multiplyXEnd{ 1.f };                  // legacy endpoint fallback: 수명 종료 시 X축 배율이다.
    float multiplyYStart{ 1.f };                // legacy endpoint fallback: 수명 시작 시 Y축 배율이다.
    float multiplyYEnd{ 1.f };                  // legacy endpoint fallback: 수명 종료 시 Y축 배율이다.
    bool multiplyX{ true };                     // false면 평가된 X 대신 1을 사용한다.
    bool multiplyY{ true };                     // false면 평가된 Y 대신 1을 사용한다.
    PointParticleSizeByLifeAxisLock axisLock{}; // Sprite 2D 범위의 X/Y 축 잠금 정책이다.
    uint32 curveKeyCount{ 2 };                  // scaleOverLife compact curve key 수다.
    Vec4 curveKeyTimes{ 0.f, 1.f, 0.f, 0.f };   // scaleOverLife key time 0..1 값이다.
    Vec4 curveKeyTimesBlock1{};                 // scaleOverLife key 4..7 time 값이다.
    Vec4 curveKeyValuesX{ 1.f, 1.f, 0.f, 0.f }; // scaleOverLife X multiplier 값이다.
    Vec4 curveKeyValuesXBlock1{};               // scaleOverLife key 4..7 X multiplier 값이다.
    Vec4 curveKeyValuesY{ 1.f, 1.f, 0.f, 0.f }; // scaleOverLife Y multiplier 값이다.
    Vec4 curveKeyValuesYBlock1{};               // scaleOverLife key 4..7 Y multiplier 값이다.
    Vec4 curveKeyArriveTangentsX{};             // scaleOverLife X key별 arrive tangent 값이다.
    Vec4 curveKeyArriveTangentsXBlock1{};       // scaleOverLife key 4..7 X arrive tangent 값이다.
    Vec4 curveKeyLeaveTangentsX{};              // scaleOverLife X key별 leave tangent 값이다.
    Vec4 curveKeyLeaveTangentsXBlock1{};        // scaleOverLife key 4..7 X leave tangent 값이다.
    Vec4 curveKeyArriveTangentsY{};             // scaleOverLife Y key별 arrive tangent 값이다.
    Vec4 curveKeyArriveTangentsYBlock1{};       // scaleOverLife key 4..7 Y arrive tangent 값이다.
    Vec4 curveKeyLeaveTangentsY{};              // scaleOverLife Y key별 leave tangent 값이다.
    Vec4 curveKeyLeaveTangentsYBlock1{};        // scaleOverLife key 4..7 Y leave tangent 값이다.
    Vec4 curveKeyModes{ 1.f, 1.f, 0.f, 0.f };   // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 curveKeyModesBlock1{};                 // scaleOverLife key 4..7 interpolation mode 값이다.
};

// 모듈별 random consumer가 lifecycle seed에 더할 수동 seed salt다.
struct PointParticleRandomSeedRuntimeDesc
{
    bool manualSeedEnabled{ false }; // true면 seed를 해당 consumer random sample에 더한다.
    uint32 seed{ 0 };                // 모듈 authoring에서 저장된 수동 seed 값이다. 0도 유효 seed다.
    bool useInstanceSeed{ false };   // true면 effect 재생 1회 seed를 해당 consumer random sample에 더한다.
};

// SpriteTilt 계열 모듈이 카드 면 기울기를 평가하기 위한 runtime payload다.
struct PointParticleSpriteTiltDesc
{
    bool initialTiltEnabled{ false };             // SpriteTilt 모듈 연결 여부다.
    Vec2 tiltDegreesMin{ 0.f, 0.f };              // spawn/stamp 시점 X/Y tilt 하한이다.
    Vec2 tiltDegreesMax{ 0.f, 0.f };              // spawn/stamp 시점 X/Y tilt 상한이다.
    PointParticleRandomSeedRuntimeDesc tiltSeed{}; // SpriteTilt random sample seed salt다.
    bool tiltOverLifeEnabled{ false };            // SpriteTiltOverLife 모듈 연결 여부다.
    bool tiltOverLifeCurveEnabled{ false };       // true면 over-life 값을 compact curve로 평가한다.
    Vec2 tiltOverLifeMin{ 0.f, 0.f };             // over-life fallback 시작 X/Y tilt다.
    Vec2 tiltOverLifeMax{ 0.f, 0.f };             // over-life fallback 종료 X/Y tilt다.
    PointParticleRandomSeedRuntimeDesc tiltOverLifeSeed{}; // Uniform over-life offset seed salt다.
    uint32 tiltOverLifeCurveKeyCount{ 2 };        // tiltOverLife compact curve key 수다.
    Vec4 tiltOverLifeCurveTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 tiltOverLifeCurveTimesBlock1{};
    Vec4 tiltOverLifeCurveValuesX{ 0.f, 0.f, 0.f, 0.f };
    Vec4 tiltOverLifeCurveValuesXBlock1{};
    Vec4 tiltOverLifeCurveValuesY{ 0.f, 0.f, 0.f, 0.f };
    Vec4 tiltOverLifeCurveValuesYBlock1{};
    Vec4 tiltOverLifeCurveArriveTangentsX{};
    Vec4 tiltOverLifeCurveArriveTangentsXBlock1{};
    Vec4 tiltOverLifeCurveLeaveTangentsX{};
    Vec4 tiltOverLifeCurveLeaveTangentsXBlock1{};
    Vec4 tiltOverLifeCurveArriveTangentsY{};
    Vec4 tiltOverLifeCurveArriveTangentsYBlock1{};
    Vec4 tiltOverLifeCurveLeaveTangentsY{};
    Vec4 tiltOverLifeCurveLeaveTangentsYBlock1{};
    Vec4 tiltOverLifeCurveModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 tiltOverLifeCurveModesBlock1{};
};

// ColorOverLife 모듈의 ConstantCurve payload를 compute shader가 평가할 수 있게 압축한 desc다.
struct PointParticleColorOverLifeCurveDesc
{
    bool colorCurveEnabled{ false };            // true면 RGB를 lifeProgress 기준 curve로 평가한다.
    bool alphaCurveEnabled{ false };            // true면 alpha를 lifeProgress 기준 curve로 평가한다.
    uint32 colorCurveKeyCount{ 2 };             // RGB compact curve key 수다.
    uint32 alphaCurveKeyCount{ 2 };             // alpha compact curve key 수다.
    Vec4 colorCurveTimes{ 0.f, 1.f, 0.f, 0.f }; // RGB key time 0..1 값이다.
    Vec4 colorCurveTimesBlock1{};
    Vec4 colorCurveValuesR{ 1.f, 1.f, 0.f, 0.f };
    Vec4 colorCurveValuesRBlock1{};
    Vec4 colorCurveValuesG{ 1.f, 1.f, 0.f, 0.f };
    Vec4 colorCurveValuesGBlock1{};
    Vec4 colorCurveValuesB{ 1.f, 1.f, 0.f, 0.f };
    Vec4 colorCurveValuesBBlock1{};
    Vec4 colorCurveArriveR{};
    Vec4 colorCurveArriveRBlock1{};
    Vec4 colorCurveArriveG{};
    Vec4 colorCurveArriveGBlock1{};
    Vec4 colorCurveArriveB{};
    Vec4 colorCurveArriveBBlock1{};
    Vec4 colorCurveLeaveR{};
    Vec4 colorCurveLeaveRBlock1{};
    Vec4 colorCurveLeaveG{};
    Vec4 colorCurveLeaveGBlock1{};
    Vec4 colorCurveLeaveB{};
    Vec4 colorCurveLeaveBBlock1{};
    Vec4 colorCurveModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 colorCurveModesBlock1{};
    Vec4 alphaCurveTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 alphaCurveTimesBlock1{};
    Vec4 alphaCurveValues{ 1.f, 1.f, 0.f, 0.f };
    Vec4 alphaCurveValuesBlock1{};
    Vec4 alphaCurveArrive{};
    Vec4 alphaCurveArriveBlock1{};
    Vec4 alphaCurveLeave{};
    Vec4 alphaCurveLeaveBlock1{};
    Vec4 alphaCurveModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 alphaCurveModesBlock1{};
};

// InitialRadialVelocity가 중심에서 방향을 만들 수 없을 때 사용할 방향 정책이다.
enum class PointParticleInitialRadialVelocityCenterDirectionMode : uint32
{
    RandomUpward = 0,
    PlaneRadial = 1,
};

enum class PointParticleSourceMotionVelocityDirectionMode : uint32
{
    InheritSourceVelocity = 0,
    SourceVelocityDirection = 1,
    SourceVelocityOpposite = 2,
    TrailTangent = 3,
    TrailTangentOpposite = 4,
    SideFromTangent = 5,
    RandomSideFromTangent = 6,
};

enum class PointParticleAccelerationTimeBasis : uint32
{
    ParticleLife = 0,
    EmitterNormalizedTime = 1,
};

enum class PointParticleVelocityScaleChannel : uint32
{
    InitialVelocity = 0,
    InitialRadialVelocity = 1,
    VelocityCone = 2,
    SourceMotionVelocity = 3,
    AccelerationIntegratedVelocity = 4,
};

// motion 계열 모듈을 compute particle update가 바로 소비할 수 있게 압축한 payload다.
struct PointParticleMotionDesc
{
    bool enabled{ false };                                          // motion 계열 모듈이 하나라도 preview에 연결되었는지 나타낸다.
    bool initialVelocityEnabled{ false };                           // 시작 벡터 속도 항을 적용할지 나타낸다.
    bool initialVelocityInWorldSpace{ false };                      // true면 시작 벡터 속도를 emitter basis 변환 없이 world 값으로 본다.
    bool initialRadialVelocityEnabled{ false };                     // 시작 방사성 속도 항을 적용할지 나타낸다.
    bool initialRadialVelocityInWorldSpace{ false };                // true면 radial pivot을 world offset으로 본다.
    bool velocityConeEnabled{ false };                              // 축 기준 cone 안에서 샘플한 시작 속도 항을 적용할지 나타낸다.
    bool velocityConeInWorldSpace{ false };                         // true면 cone axis를 emitter basis 변환 없이 world 값으로 본다.
    bool sourceMotionVelocityEnabled{ false };                      // source/trail motion 기반 시작 속도 항을 적용할지 나타낸다.
    bool accelerationInWorldSpace{ true };                          // true면 Acceleration 벡터를 emitter basis 변환 없이 world 값으로 본다.
    Vec3 initialVelocityMin{ 0.f, 0.f, 0.f };                       // InitialVelocity vector 분포의 하한이다.
    Vec3 initialVelocityMax{ 0.f, 0.f, 0.f };                       // InitialVelocity vector 분포의 상한이다.
    Vec3 radialPivot{ 0.f, 0.f, 0.f };                              // InitialRadialVelocity의 방사 방향 기준점이다.
    Vec2 radialSpeed{ 0.f, 0.f };                                   // InitialRadialVelocity speed 분포의 min/max다.
    Vec3 velocityConeAxis{ 0.f, 1.f, 0.f };                         // VelocityCone의 중심축이다.
    float velocityConeAngleDegrees{ 30.f };                         // VelocityCone의 중심축 기준 half-angle이다.
    Vec2 velocityConeSpeed{ 0.f, 0.f };                             // VelocityCone speed 분포의 min/max다.
    PointParticleSourceMotionVelocityDirectionMode sourceMotionVelocityDirectionMode{
        PointParticleSourceMotionVelocityDirectionMode::TrailTangent
    };                                                              // SourceMotionVelocity가 source motion에서 방향을 고르는 정책이다.
    Vec2 sourceMotionVelocitySpeed{ 0.f, 0.f };                     // SourceMotionVelocity speed 분포의 min/max다.
    float sourceMotionVelocitySourceSpeedScale{ 0.f };              // source speed를 시작 속도 크기에 더할 배율이다.
    float sourceMotionVelocitySpreadAngleDegrees{ 0.f };            // source motion 방향 주변 local 분산 half-angle이다.
    PointParticleInitialRadialVelocityCenterDirectionMode initialRadialVelocityCenterDirectionMode{
        PointParticleInitialRadialVelocityCenterDirectionMode::RandomUpward
    };                                                              // 중심 중첩 시 InitialRadialVelocity가 사용할 방향 정책이다.
    Vec3 accelerationMin{ 0.f, 0.f, 0.f };                          // Acceleration 분포의 하한이다.
    Vec3 accelerationMax{ 0.f, 0.f, 0.f };                          // Acceleration 분포의 상한이다.
    PointParticleAccelerationTimeBasis accelerationTimeBasis{
        PointParticleAccelerationTimeBasis::ParticleLife
    };                                                              // Acceleration ConstantCurve를 평가할 시간 기준이다.
    bool accelerationCurveEnabled{ false };                         // true면 Acceleration ConstantCurve를 선택된 시간 기준으로 절대 가속 평가한다.
    uint32 accelerationCurveKeyCount{ 2 };                          // Acceleration compact curve key 수다.
    Vec4 accelerationCurveTimes{ 0.f, 1.f, 0.f, 0.f };              // Acceleration key time 0..1 값이다.
    Vec4 accelerationCurveTimesBlock1{};                            // Acceleration key 4..7 time 값이다.
    Vec4 accelerationCurveValuesX{ 0.f, 0.f, 0.f, 0.f };            // Acceleration key별 X 값이다.
    Vec4 accelerationCurveValuesXBlock1{};                          // Acceleration key 4..7 X 값이다.
    Vec4 accelerationCurveValuesY{ 0.f, 0.f, 0.f, 0.f };            // Acceleration key별 Y 값이다.
    Vec4 accelerationCurveValuesYBlock1{};                          // Acceleration key 4..7 Y 값이다.
    Vec4 accelerationCurveValuesZ{ 0.f, 0.f, 0.f, 0.f };            // Acceleration key별 Z 값이다.
    Vec4 accelerationCurveValuesZBlock1{};                          // Acceleration key 4..7 Z 값이다.
    Vec4 accelerationCurveArriveTangentsX{};                        // Acceleration key별 X arrive tangent 값이다.
    Vec4 accelerationCurveArriveTangentsXBlock1{};                  // Acceleration key 4..7 X arrive tangent 값이다.
    Vec4 accelerationCurveLeaveTangentsX{};                         // Acceleration key별 X leave tangent 값이다.
    Vec4 accelerationCurveLeaveTangentsXBlock1{};                   // Acceleration key 4..7 X leave tangent 값이다.
    Vec4 accelerationCurveArriveTangentsY{};                        // Acceleration key별 Y arrive tangent 값이다.
    Vec4 accelerationCurveArriveTangentsYBlock1{};                  // Acceleration key 4..7 Y arrive tangent 값이다.
    Vec4 accelerationCurveLeaveTangentsY{};                         // Acceleration key별 Y leave tangent 값이다.
    Vec4 accelerationCurveLeaveTangentsYBlock1{};                   // Acceleration key 4..7 Y leave tangent 값이다.
    Vec4 accelerationCurveArriveTangentsZ{};                        // Acceleration key별 Z arrive tangent 값이다.
    Vec4 accelerationCurveArriveTangentsZBlock1{};                  // Acceleration key 4..7 Z arrive tangent 값이다.
    Vec4 accelerationCurveLeaveTangentsZ{};                         // Acceleration key별 Z leave tangent 값이다.
    Vec4 accelerationCurveLeaveTangentsZBlock1{};                   // Acceleration key 4..7 Z leave tangent 값이다.
    Vec4 accelerationCurveModes{ 1.f, 1.f, 0.f, 0.f };              // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 accelerationCurveModesBlock1{};                            // Acceleration key 4..7 interpolation mode 값이다.
    Vec2 drag{ 0.f, 0.f };                                          // Drag 분포의 min/max 감쇠값이다.
    Vec2 velocityScaleByLife{ 1.f, 1.f };                           // VelocityOverLife의 시작/종료 fallback 배율이다.
    uint32 velocityScaleByLifeCurveKeyCount{ 2 };                   // VelocityOverLife compact curve key 수다.
    Vec4 velocityScaleByLifeCurveTimes{ 0.f, 1.f, 0.f, 0.f };       // VelocityOverLife key time 0..1 값이다.
    Vec4 velocityScaleByLifeCurveTimesBlock1{};                     // VelocityOverLife key 4..7 time 값이다.
    Vec4 velocityScaleByLifeCurveValues{ 1.f, 1.f, 0.f, 0.f };      // VelocityOverLife key별 속도 배율 값이다.
    Vec4 velocityScaleByLifeCurveValuesBlock1{};                    // VelocityOverLife key 4..7 속도 배율 값이다.
    Vec4 velocityScaleByLifeCurveArriveTangents{};                  // VelocityOverLife key별 arrive tangent 값이다.
    Vec4 velocityScaleByLifeCurveArriveTangentsBlock1{};            // VelocityOverLife key 4..7 arrive tangent 값이다.
    Vec4 velocityScaleByLifeCurveLeaveTangents{};                   // VelocityOverLife key별 leave tangent 값이다.
    Vec4 velocityScaleByLifeCurveLeaveTangentsBlock1{};             // VelocityOverLife key 4..7 leave tangent 값이다.
    Vec4 velocityScaleByLifeCurveModes{ 1.f, 1.f, 0.f, 0.f };       // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 velocityScaleByLifeCurveModesBlock1{};                     // VelocityOverLife key 4..7 interpolation mode 값이다.
    PointParticleRandomSeedRuntimeDesc initialVelocitySeed{};       // InitialVelocity random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc initialRadialVelocitySeed{}; // InitialRadialVelocity random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc velocityConeSeed{};          // VelocityCone random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc sourceMotionVelocitySeed{};  // SourceMotionVelocity speed/spread random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc accelerationSeed{};          // Acceleration random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc dragSeed{};                  // Drag random sample seed salt다.
    PointParticleFloatCurveRuntimeDesc initialVelocityScaleByLife{}; // InitialVelocity channel 전용 VelocityOverLife 배율이다.
    PointParticleFloatCurveRuntimeDesc initialRadialVelocityScaleByLife{}; // InitialRadialVelocity channel 전용 VelocityOverLife 배율이다.
    PointParticleFloatCurveRuntimeDesc velocityConeScaleByLife{};    // VelocityCone channel 전용 VelocityOverLife 배율이다.
    PointParticleFloatCurveRuntimeDesc sourceMotionVelocityScaleByLife{}; // SourceMotionVelocity channel 전용 VelocityOverLife 배율이다.
    PointParticleFloatCurveRuntimeDesc accelerationIntegratedVelocityScaleByLife{}; // Acceleration 누적 속도 channel 전용 VelocityOverLife 배율이다.
};

// rotation 계열 모듈을 compute particle update가 바로 소비할 수 있게 압축한 payload다.
struct PointParticleRotationDesc
{
    bool enabled{ false };                                         // rotation 계열 모듈이 하나라도 preview에 연결되었는지 나타낸다.
    Vec2 initialRotationDegrees{ 0.f, 0.f };                       // InitialRotation 분포의 min/max 각도다.
    Vec2 rotationOverLifeDegrees{ 0.f, 0.f };                      // RotationOverLife curve의 시작/종료 fallback 추가 각도다.
    Vec2 initialRotationRateDegrees{ 0.f, 0.f };                   // InitialRotationRate 분포의 min/max 초당 각도다.
    Vec2 rotationRateScaleByLife{ 1.f, 1.f };                      // RotationRateScaleByLife의 시작/종료 배율이다.
    uint32 rotationOverLifeCurveKeyCount{ 2 };                     // RotationOverLife compact curve key 수다.
    Vec4 rotationOverLifeCurveTimes{ 0.f, 1.f, 0.f, 0.f };         // RotationOverLife key time 0..1 값이다.
    Vec4 rotationOverLifeCurveTimesBlock1{};                       // RotationOverLife key 4..7 time 값이다.
    Vec4 rotationOverLifeCurveValues{ 0.f, 0.f, 0.f, 0.f };        // RotationOverLife key별 추가 회전각 degree 값이다.
    Vec4 rotationOverLifeCurveValuesBlock1{};                      // RotationOverLife key 4..7 추가 회전각 degree 값이다.
    Vec4 rotationOverLifeCurveArriveTangents{};                    // RotationOverLife key별 arrive tangent 값이다.
    Vec4 rotationOverLifeCurveArriveTangentsBlock1{};              // RotationOverLife key 4..7 arrive tangent 값이다.
    Vec4 rotationOverLifeCurveLeaveTangents{};                     // RotationOverLife key별 leave tangent 값이다.
    Vec4 rotationOverLifeCurveLeaveTangentsBlock1{};               // RotationOverLife key 4..7 leave tangent 값이다.
    Vec4 rotationOverLifeCurveModes{ 1.f, 1.f, 0.f, 0.f };         // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 rotationOverLifeCurveModesBlock1{};                       // RotationOverLife key 4..7 interpolation mode 값이다.
    uint32 rotationRateScaleByLifeCurveKeyCount{ 2 };              // RotationRateScaleByLife compact curve key 수다.
    Vec4 rotationRateScaleByLifeCurveTimes{ 0.f, 1.f, 0.f, 0.f };  // RotationRateScaleByLife key time 0..1 값이다.
    Vec4 rotationRateScaleByLifeCurveTimesBlock1{};                // RotationRateScaleByLife key 4..7 time 값이다.
    Vec4 rotationRateScaleByLifeCurveValues{ 1.f, 1.f, 0.f, 0.f }; // RotationRateScaleByLife key별 배율 값이다.
    Vec4 rotationRateScaleByLifeCurveValuesBlock1{};               // RotationRateScaleByLife key 4..7 배율 값이다.
    Vec4 rotationRateScaleByLifeCurveArriveTangents{};             // RotationRateScaleByLife key별 arrive tangent 값이다.
    Vec4 rotationRateScaleByLifeCurveArriveTangentsBlock1{};       // RotationRateScaleByLife key 4..7 arrive tangent 값이다.
    Vec4 rotationRateScaleByLifeCurveLeaveTangents{};              // RotationRateScaleByLife key별 leave tangent 값이다.
    Vec4 rotationRateScaleByLifeCurveLeaveTangentsBlock1{};        // RotationRateScaleByLife key 4..7 leave tangent 값이다.
    Vec4 rotationRateScaleByLifeCurveModes{ 1.f, 1.f, 0.f, 0.f };  // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 rotationRateScaleByLifeCurveModesBlock1{};                // RotationRateScaleByLife key 4..7 interpolation mode 값이다.
    PointParticleRandomSeedRuntimeDesc initialRotationSeed{};      // InitialRotation random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc initialRotationRateSeed{};  // InitialRotationRate random sample seed salt다.
};

// Spawn 모듈의 burst 항목을 runtime emitter가 소비할 수 있게 압축한 payload다.
struct PointParticleBurstDesc
{
    float time{ 0.f };   // emitter 시작 후 burst가 실행될 시간이다.
    uint32 count{ 16u }; // 해당 시간에 추가로 활성화할 particle 수다.
};

// Spawn 모듈이 만든 연속/버스트 생성 규칙이다.
struct PointParticleSpawnDesc
{
    bool processSpawnRate{ true };                           // 연속 spawn rate를 적용할지 나타낸다.
    float spawnRate{ 10.f };                                 // 초당 생성할 particle 수다.
    Vec2 spawnRateRange{ 10.f, 10.f };                       // loop 시작 시 seed 기반으로 샘플할 spawn rate 범위다.
    PointParticleRandomSeedRuntimeDesc spawnRateSeed{};      // SpawnRate random sample seed salt다.
    float spawnRateScale{ 1.f };                             // spawn rate에 곱할 배율이다.
    Vec2 spawnRateScaleRange{ 1.f, 1.f };                    // loop 시작 시 seed 기반으로 샘플할 spawn rate scale 범위다.
    PointParticleRandomSeedRuntimeDesc spawnRateScaleSeed{}; // SpawnRateScale random sample seed salt다.
    bool processBurstList{ true };                           // burst 목록을 적용할지 나타낸다.
    vector<PointParticleBurstDesc> bursts{};                 // emitter time 기준 1회성 burst 목록이다.
    float burstScale{ 1.f };                                 // burst count에 곱할 배율이다.
    Vec2 burstScaleRange{ 1.f, 1.f };                        // burst 실행 시점에 seed 기반으로 샘플할 burst scale 범위다.
    PointParticleRandomSeedRuntimeDesc burstScaleSeed{};     // BurstScale random sample seed salt다.
    uint32 maxActiveCount{ 64u };                            // 동시에 살아 있을 수 있는 particle 상한이다.
    PointParticleFloatCurveRuntimeDesc spawnRateCurve{};      // SpawnRate ConstantCurve를 loop phase 기준으로 평가하는 payload다.
    PointParticleFloatCurveRuntimeDesc spawnRateScaleCurve{}; // SpawnRateScale ConstantCurve를 loop phase 기준으로 평가하는 payload다.
    PointParticleFloatCurveRuntimeDesc burstScaleCurve{};     // BurstScale ConstantCurve를 burst time phase 기준으로 평가하는 payload다.
};

// InitialLocation 모듈이 만든 생성 시점 위치 offset 분포다.
struct PointParticleInitialLocationDesc
{
    bool enabled{ false };                 // authoring InitialLocation 모듈이 preview/runtime에 연결되었는지 나타낸다.
    Vec3 minOffset{ -0.5f, -0.5f, -0.5f }; // emitter center 기준 최소 시작 offset이다.
    Vec3 maxOffset{ 0.5f, 0.5f, 0.5f };    // emitter center 기준 최대 시작 offset이다.
};

// SphereLocation 모듈이 구 내부/표면 중 어디에서 생성 위치를 샘플링할지 결정한다.
enum class PointParticleSphereLocationMode : uint32
{
    Volume = 0,  // 구 내부 부피에서 샘플링한다.
    Surface = 1, // 구 표면에서 샘플링한다.
};

// SphereLocation 모듈이 Surface 위치를 고르는 방식이다.
enum class PointParticleSphereLocationPlacementMode : uint32
{
    Random = 0,
    EvenByParticleIndex = 1,
};

// SphereLocation 모듈이 만든 생성 시점 구형 위치 offset 분포다.
struct PointParticleSphereLocationDesc
{
    bool enabled{ false };                                  // authoring SphereLocation 모듈이 preview/runtime에 연결되었는지 나타낸다.
    Vec3 offset{ 0.f, 0.f, 0.f };                           // emitter center 기준 구 중심 offset이다.
    float radius{ 1.f };                                    // 구 샘플링 반지름이다. 0 이하면 no-op으로 처리한다.
    PointParticleSphereLocationMode mode{
        PointParticleSphereLocationMode::Volume };          // 구 내부/표면 샘플링 정책이다.
    PointParticleSphereLocationPlacementMode placementMode{
        PointParticleSphereLocationPlacementMode::Random }; // Surface 모드에서 구 표면 방향을 고르는 정책이다.
};

// PlaneRadialLocation 모듈이 위치를 샘플링할 emitter-local 평면이다.
enum class PointParticlePlaneRadialLocationPlane : uint32
{
    XY = 0,
    XZ = 1,
    YZ = 2,
    CameraFacing = 3,
};

// PlaneRadialLocation 모듈이 평면 위에서 사용할 샘플링 형태다.
enum class PointParticlePlaneRadialLocationShape : uint32
{
    Rectangle = 0,
    Disc = 1,
    Ring = 2,
    Arc = 3,
};

// PlaneRadialLocation 모듈이 polar angle을 고르는 방식이다.
enum class PointParticlePlaneRadialLocationPlacementMode : uint32
{
    Random = 0,
    EvenByParticleIndex = 1,
};

// PlaneRadialLocation 모듈이 만든 생성 시점 평면/방사 위치 offset 분포다.
struct PointParticlePlaneRadialLocationDesc
{
    bool enabled{ false };                                       // PlaneRadialLocation 모듈 연결 여부다.
    PointParticlePlaneRadialLocationPlane plane{
        PointParticlePlaneRadialLocationPlane::XY };             // 샘플링할 emitter-local 평면이다.
    PointParticlePlaneRadialLocationShape shape{
        PointParticlePlaneRadialLocationShape::Disc };           // 평면 위 위치 샘플링 형태다.
    PointParticlePlaneRadialLocationPlacementMode placementMode{
        PointParticlePlaneRadialLocationPlacementMode::Random }; // polar angle 선택 정책이다.
    Vec3 offset{ 0.f, 0.f, 0.f };                                // emitter center 기준 평면 샘플 중심 offset이다.
    Vec2 uRange{ -0.5f, 0.5f };                                  // Rectangle 샘플링에서 plane U축 범위다.
    Vec2 vRange{ -0.5f, 0.5f };                                  // Rectangle 샘플링에서 plane V축 범위다.
    Vec2 radiusRange{ 0.f, 1.f };                                // Disc/Ring/Arc 샘플링 반지름 범위다.
    Vec2 angleDegreesRange{ 0.f, 360.f };                        // Disc/Ring/Arc 샘플링 각도 범위다.
    PointParticleRandomSeedRuntimeDesc uSeed{};                  // Rectangle U random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc vSeed{};                  // Rectangle V random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc radiusSeed{};             // Polar radius random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc angleDegreesSeed{};       // Polar angle random sample seed salt다.
    float thickness{ 0.f };                                      // plane normal 방향 두께다. 0이면 평면에 붙는다.
};

// CylinderLocation 모듈이 원통 길이축으로 사용할 emitter-local axis다.
enum class PointParticleCylinderLocationAxis : uint32
{
    LocalX = 0,
    LocalY = 1,
    LocalLook = 2,
};

// CylinderLocation 모듈이 원통에서 위치를 샘플링할 영역이다.
enum class PointParticleCylinderLocationMode : uint32
{
    SideSurface = 0,
    Volume = 1,
};

// CylinderLocation 모듈이 원주 angle을 고르는 방식이다.
enum class PointParticleCylinderLocationPlacementMode : uint32
{
    Random = 0,
    EvenByParticleIndex = 1,
};

// CylinderLocation 모듈이 만든 생성 시점 원통 위치 offset 분포다.
struct PointParticleCylinderLocationDesc
{
    bool enabled{ false };                                           // CylinderLocation 모듈 연결 여부다.
    PointParticleCylinderLocationAxis axis{
        PointParticleCylinderLocationAxis::LocalLook };              // 원통 길이축으로 사용할 emitter-local axis다.
    PointParticleCylinderLocationMode mode{
        PointParticleCylinderLocationMode::SideSurface };            // 원통 옆면/shell 또는 부피 샘플링 정책이다.
    PointParticleCylinderLocationPlacementMode placementMode{
        PointParticleCylinderLocationPlacementMode::Random };        // 원주 angle 선택 정책이다.
    Vec3 offset{ 0.f, 0.f, 0.f };                                    // emitter center 기준 원통 중심 offset이다.
    Vec2 radiusRange{ 0.f, 1.f };                                    // 원통 반지름 또는 shell 두께 범위다.
    Vec2 heightRange{ -0.5f, 0.5f };                                 // 원통 길이축 방향 위치 범위다.
    Vec2 angleDegreesRange{ 0.f, 360.f };                            // 원주 angle 범위다.
    PointParticleRandomSeedRuntimeDesc radiusSeed{};                 // radius random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc heightSeed{};                 // height random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc angleDegreesSeed{};           // angle random sample seed salt다.
};

// CylinderOrientation 모듈이 cylinder sample frame에서 고를 방향이다.
enum class PointParticleCylinderOrientationMode : uint32
{
    None = 0,
    FaceRadialOut = 1,
    FaceRadialIn = 2,
    FaceTangentCW = 3,
    FaceTangentCCW = 4,
    FaceCylinderAxisPositive = 5,
    FaceCylinderAxisNegative = 6,
};

// PlaneRadialOrientation 모듈이 회전 결과를 적용할 runtime 대상이다.
enum class PointParticlePlaneRadialOrientationTargetKind : uint32
{
    Auto = 0,
    Sprite2D = 1,
    Mesh3D = 2,
};

// PlaneRadialOrientation 모듈이 radial sample frame에서 고를 방향이다.
enum class PointParticlePlaneRadialOrientationMode : uint32
{
    None = 0,
    FaceRadialOut = 1,
    FaceRadialIn = 2,
    FaceTangentCW = 3,
    FaceTangentCCW = 4,
    FacePlaneNormal = 5,
};

// PlaneRadialOrientation 모듈이 mesh local 축을 지정할 때 사용하는 축 token이다.
enum class PointParticlePlaneRadialOrientationAxis : uint32
{
    PositiveX = 0,
    NegativeX = 1,
    PositiveY = 2,
    NegativeY = 3,
    PositiveZ = 4,
    NegativeZ = 5,
};

// PlaneRadialOrientation 모듈이 만든 radial frame 기반 초기 회전 payload다.
struct PointParticlePlaneRadialOrientationDesc
{
    bool enabled{ false };                                     // PlaneRadialOrientation 모듈 연결 여부다.
    PointParticlePlaneRadialOrientationTargetKind targetKind{
        PointParticlePlaneRadialOrientationTargetKind::Auto }; // 회전 적용 대상 자동/스프라이트/메시 정책이다.
    PointParticlePlaneRadialOrientationMode orientationMode{
        PointParticlePlaneRadialOrientationMode::None };       // radial frame에서 고를 초기 방향이다.
    PointParticlePlaneRadialOrientationAxis meshForwardAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveZ };  // mesh forward로 볼 local 축이다.
    PointParticlePlaneRadialOrientationAxis meshUpAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveY };  // mesh up으로 볼 local 축이다.
    float tiltDegrees{ 0.f };                                  // radial 방향에서 추가로 기울일 각도다.
    float rollOffsetDegrees{ 0.f };                            // 최종 방향 기준 roll 보정 각도다.
};

// SphereRadialOrientation 모듈이 sphere sample frame에서 고를 방향이다.
enum class PointParticleSphereRadialOrientationMode : uint32
{
    None = 0,
    FaceRadialOut = 1,
    FaceRadialIn = 2,
};

// SphereRadialOrientation 모듈이 만든 구형 radial frame 기반 초기 mesh 회전 payload다.
struct PointParticleSphereRadialOrientationDesc
{
    bool enabled{ false };                                    // SphereRadialOrientation 모듈 연결 여부다.
    PointParticleSphereRadialOrientationMode orientationMode{
        PointParticleSphereRadialOrientationMode::None };     // sphere radial frame에서 고를 초기 방향이다.
    PointParticlePlaneRadialOrientationAxis meshForwardAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveZ }; // mesh forward로 볼 local 축이다.
    PointParticlePlaneRadialOrientationAxis meshUpAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveY }; // mesh up으로 볼 local 축이다.
    float tiltDegrees{ 0.f };                                 // radial 방향에서 추가로 기울일 각도다.
    float rollOffsetDegrees{ 0.f };                           // 최종 방향 기준 roll 보정 각도다.
};

// CylinderOrientation 모듈이 만든 cylinder sample frame 기반 초기 회전 payload다.
struct PointParticleCylinderOrientationDesc
{
    bool enabled{ false };                                     // CylinderOrientation 모듈 연결 여부다.
    PointParticlePlaneRadialOrientationTargetKind targetKind{
        PointParticlePlaneRadialOrientationTargetKind::Auto }; // 회전 적용 대상 자동/스프라이트/메시 정책이다.
    PointParticleCylinderOrientationMode orientationMode{
        PointParticleCylinderOrientationMode::None };          // cylinder frame에서 고를 초기 방향이다.
    bool followOrbitOverLife{ false };                         // OrbitOverLife 위치 회전을 sprite 방향에도 반영할지 여부다.
    PointParticlePlaneRadialOrientationAxis meshForwardAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveZ };  // mesh forward로 볼 local 축이다.
    PointParticlePlaneRadialOrientationAxis meshUpAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveY };  // mesh up으로 볼 local 축이다.
    float tiltDegrees{ 0.f };                                  // cylinder orientation 결과에 더하는 pitch/tilt 보정이다.
    float rollOffsetDegrees{ 0.f };                            // 최종 방향 기준 roll 보정 각도다.
};

// OrbitOverLife 모듈이 emitter-local offset을 회전시킬 plane이다.
enum class EffectOrbitPlane : uint32
{
    XY = 0,
    XZ = 1,
    YZ = 2,
};

// OrbitOverLife 모듈을 sprite/mesh runtime이 공유해 소비하는 payload다.
struct EffectOrbitOverLifeRuntimeDesc
{
    bool enabled{ false };                             // OrbitOverLife 모듈이 preview/runtime에 연결되었는지 나타낸다.
    EffectOrbitPlane plane{ EffectOrbitPlane::XY };    // emitter-local offset을 회전시킬 평면이다.
    Vec2 angleDegreesOverLife{ 0.f, 0.f };             // 수명 시작/종료 시 누적 orbit 각도 fallback이다.
    uint32 angleCurveKeyCount{ 2 };                    // orbit angle compact curve key 수다.
    Vec4 angleCurveTimes{ 0.f, 1.f, 0.f, 0.f };        // orbit angle key time 0..1 값이다.
    Vec4 angleCurveTimesBlock1{};                      // orbit angle key 4..7 time 값이다.
    Vec4 angleCurveValues{ 0.f, 0.f, 0.f, 0.f };       // orbit angle key별 degree 값이다.
    Vec4 angleCurveValuesBlock1{};                     // orbit angle key 4..7 degree 값이다.
    Vec4 angleCurveArriveTangents{};                   // orbit angle key별 arrive tangent 값이다.
    Vec4 angleCurveArriveTangentsBlock1{};             // orbit angle key 4..7 arrive tangent 값이다.
    Vec4 angleCurveLeaveTangents{};                    // orbit angle key별 leave tangent 값이다.
    Vec4 angleCurveLeaveTangentsBlock1{};              // orbit angle key 4..7 leave tangent 값이다.
    Vec4 angleCurveModes{ 1.f, 1.f, 0.f, 0.f };        // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 angleCurveModesBlock1{};                      // orbit angle key 4..7 interpolation mode 값이다.
    Vec2 radiusScaleOverLife{ 1.f, 1.f };              // 수명 시작/종료 시 orbit radius 배율 fallback이다.
    uint32 radiusScaleCurveKeyCount{ 2 };              // radius scale compact curve key 수다.
    Vec4 radiusScaleCurveTimes{ 0.f, 1.f, 0.f, 0.f };  // radius scale key time 0..1 값이다.
    Vec4 radiusScaleCurveTimesBlock1{};                // radius scale key 4..7 time 값이다.
    Vec4 radiusScaleCurveValues{ 1.f, 1.f, 0.f, 0.f }; // radius scale key별 배율 값이다.
    Vec4 radiusScaleCurveValuesBlock1{};               // radius scale key 4..7 배율 값이다.
    Vec4 radiusScaleCurveArriveTangents{};             // radius scale key별 arrive tangent 값이다.
    Vec4 radiusScaleCurveArriveTangentsBlock1{};       // radius scale key 4..7 arrive tangent 값이다.
    Vec4 radiusScaleCurveLeaveTangents{};              // radius scale key별 leave tangent 값이다.
    Vec4 radiusScaleCurveLeaveTangentsBlock1{};        // radius scale key 4..7 leave tangent 값이다.
    Vec4 radiusScaleCurveModes{ 1.f, 1.f, 0.f, 0.f };  // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 radiusScaleCurveModesBlock1{};                // radius scale key 4..7 interpolation mode 값이다.
};

// 인스턴스 생성에 공통으로 필요한 spawn 규칙.
struct InstanceSpawnDesc
{
    uint32 instanceCount{}; // 생성할 인스턴스 수.
    Vec3 center{};          // 스폰 영역 중심.
    Vec3 range{};           // 스폰 영역 크기.
    Vec2 scale{};           // 랜덤 스케일 범위.
};

// Deprecated: CPU particle backend는 legacy fallback으로만 유지한다. 신규 Sprite runtime은 ComputePointParticleDesc를 사용한다.
struct [[deprecated("Use compute sprite particle path instead.")]] PointParticleDesc : public InstanceSpawnDesc
{
    Vec3 pivot{};                                                    // 퍼짐 기준점.
    Vec2 speed{};                                                    // 랜덤 속도 범위.
    Vec2 lifeTime{};                                                 // 랜덤 생존 시간 범위.
    Vec4 startColorMin{ 1.f, 1.f, 1.f, 1.f };                        // Initial Color에서 샘플링할 시작 색상 하한.
    Vec4 startColorMax{ 1.f, 1.f, 1.f, 1.f };                        // Initial Color에서 샘플링할 시작 색상 상한.
    Vec4 endColorMin{ 1.f, 1.f, 1.f, 1.f };                          // Color Over Life에서 샘플링할 종료 색상 하한.
    Vec4 endColorMax{ 1.f, 1.f, 1.f, 1.f };                          // Color Over Life에서 샘플링할 종료 색상 상한.
    uint32 subUVRows{ 1 };                                           // atlas row 수다.
    uint32 subUVCols{ 1 };                                           // atlas column 수다.
    PointParticleSubUVFrameOverLifeDesc subUvFrameOverLife{};        // 수명 진행률 기준 frame 선택 규칙이다.
    PointParticleSizeByLifeDesc sizeByLife{};                        // 수명 진행률 기준 크기 배율 규칙이다.
    bool loop{};                                                     // true면 생존 시간이 끝난 인스턴스를 재생성한다.
    PointParticleDrawMode drawMode{ PointParticleDrawMode::Direct }; // Stage2 indirect draw 검증용 제출 방식.
};

// compute shader가 instance payload를 갱신하는 compute-first sprite particle desc.
struct ComputePointParticleDesc : public InstanceSpawnDesc
{
    Vec2 sizeMin{ 0.12f, 0.12f };                                     // particle 기본 가로/세로 크기 하한이다.
    Vec2 sizeMax{ 0.28f, 0.28f };                                     // particle 기본 가로/세로 크기 상한이다.
    PointParticleRandomSeedRuntimeDesc initialSizeSeed{};             // InitialSize random sample seed salt다.
    Vec2 lifeTime{};                                                  // compute shader가 누적 시간을 계산할 때 사용할 생존 시간 범위.
    PointParticleFloatCurveRuntimeDesc lifetimeCurve{};               // Lifetime ConstantCurve를 spawn event phase 기준으로 평가하는 payload다.
    Vec4 startColorMin{ 1.f, 1.f, 1.f, 1.f };                         // compute sprite preview 시작 색상/알파 하한.
    Vec4 startColorMax{ 1.f, 1.f, 1.f, 1.f };                         // compute sprite preview 시작 색상/알파 상한.
    PointParticleRandomSeedRuntimeDesc initialColorSeed{};            // InitialColor RGB random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc initialAlphaSeed{};            // InitialColor Alpha random sample seed salt다.
    Vec4 endColorMin{ 1.f, 1.f, 1.f, 1.f };                           // compute sprite preview 종료 색상/알파 하한.
    Vec4 endColorMax{ 1.f, 1.f, 1.f, 1.f };                           // compute sprite preview 종료 색상/알파 상한.
    PointParticleRandomSeedRuntimeDesc colorOverLifeSeed{};           // ColorOverLife RGB random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc alphaOverLifeSeed{};           // ColorOverLife Alpha random sample seed salt다.
    PointParticleColorOverLifeCurveDesc colorOverLifeCurve{};         // ConstantCurve ColorOverLife payload다.
    Vec4 coreColorRgbParticleLifeUniformParams{};                     // x: enabled.
    Vec4 coreColorRgbParticleLifeUniformMin{ 1.f, 0.85f, 0.45f, 0.f };
    Vec4 coreColorRgbParticleLifeUniformMax{ 1.f, 0.85f, 0.45f, 0.f };
    XMUINT4 coreColorRgbParticleLifeUniformSeed{};                    // x: seed salt after playback-seed resolution.
    Vec2 radius{};                                                    // 화면 확인용 원형 이동 반경 범위. authoring 의미로 승격하지 않는다.
    uint32 subUVRows{ 1 };                                            // atlas row 수다.
    uint32 subUVCols{ 1 };                                            // atlas column 수다.
    PointParticleSubUVFrameOverLifeDesc subUvFrameOverLife{};         // 수명 진행률 기준 frame 선택 규칙이다.
    PointParticleSizeByLifeDesc sizeByLife{};                         // 수명 진행률 기준 크기 배율 규칙이다.
    PointParticleMotionDesc motion{};                                 // motion 모듈들이 만든 compute update 규칙이다.
    float playbackDuration{ 1.f };                                    // emitter time 기준 curve 평가에 사용할 loop duration이다.
    EffectOrbitOverLifeRuntimeDesc orbitOverLife{};                   // OrbitOverLife 모듈이 만든 emitter-local pivot 회전 payload다.
    PointParticleRotationDesc rotation{};                             // rotation 모듈들이 만든 sprite 회전 규칙이다.
    PointParticleSpriteTiltDesc spriteTilt{};                         // SpriteTilt 모듈들이 만든 카드 면 기울기 규칙이다.
    PointParticleSpawnDesc spawn{};                                   // Spawn 모듈이 만든 활성화/버스트 생성 규칙이다.
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};                // Lifetime random sample seed salt다.
    PointParticleInitialLocationDesc initialLocation{};               // InitialLocation 모듈이 만든 시작 위치 offset 분포다.
    PointParticleSphereLocationDesc sphereLocation{};                 // SphereLocation 모듈이 만든 시작 위치 구형 offset 분포다.
    PointParticlePlaneRadialLocationDesc planeRadialLocation{};       // PlaneRadialLocation 모듈이 만든 시작 위치 평면/방사 offset 분포다.
    PointParticleCylinderLocationDesc cylinderLocation{};             // CylinderLocation 모듈이 만든 시작 위치 원통 offset 분포다.
    PointParticlePlaneRadialOrientationDesc planeRadialOrientation{}; // PlaneRadialOrientation 모듈이 만든 sprite 2D 초기 회전 payload다.
    PointParticleCylinderOrientationDesc cylinderOrientation{};        // CylinderOrientation 모듈이 만든 cylinder frame 기반 sprite 방향 payload다.
    PointParticleRandomSeedRuntimeDesc initialLocationSeed{};         // InitialLocation random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc sphereLocationSeed{};          // SphereLocation random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{};        // SubUV RandomFrame random sample seed salt다.
    Vec3 emitterRight{ 1.f, 0.f, 0.f };                               // emitter local X축을 world 방향으로 변환할 basis다.
    Vec3 emitterUp{ 0.f, 1.f, 0.f };                                  // emitter local Y축을 world 방향으로 변환할 basis다.
    Vec3 emitterLook{ 0.f, 0.f, 1.f };                                // emitter local Z축을 world 방향으로 변환할 basis다.
    uint32 effectPlaybackSeed{ 0 };                                   // effect root 재생 1회 단위 random seed다.
    bool useMaxDrawCount{ false };                                    // true면 draw 제출 수만 별도로 제한한다.
    uint32 maxDrawCount{ 0 };                                         // 0이면 instanceCount 기준으로 제한한다.
    PointParticleDrawMode drawMode{                                   // compute particle 경로의 정식 draw 제출 방식.
        PointParticleDrawMode::DrawIndexedInstancedIndirect };
    uint32 computeShaderLevelIndex{};                                 // compute shader prototype 등록 level index다.
    wstring computeShaderPrototypeTag{};                              // instance payload를 갱신할 compute shader component prototype tag.
};

NS_END
