#pragma once
#include <array>
#include <variant>
#include "EffectMaterial_Types.h"
#include "Engine_Typedef.h"
#include "GameObject.h"
#include "Particle_Types.h"

NS_BEGIN(Engine)

//## Emitter::Type

// Effect 내부 emitter의 concrete runtime 종류를 구분한다.
enum class EffectEmitterKind : uint8
{
    ComputeSprite,       // compute-first sprite emitter runtime이다.
    ComputeTrail,        // compute-first trail emitter runtime이다.
    SourceHistoryRibbon, // source/root position history 기반 camera-facing ribbon runtime이다.
    Mesh,                // model mesh를 instance buffer로 그리는 mesh emitter runtime이다.
};

// Emitter 단위로 자동 render group 판정을 덮을 수 있는 제한된 override다.
enum class EffectEmitterRenderLayerOverride : uint8
{
    Auto,
    UIEffect,
};

enum class EffectSortPolicy : uint8
{
    None,
    EmitterDepth,
};

//## Trail

// Trail runtime이 한 프레임에서 소비할 base/tip world sample이다.
struct EffectTrailSample
{
    Vec3 baseWorldPosition{}; // trail 폭의 시작 기준점이다.
    Vec3 tipWorldPosition{};  // trail 폭의 끝 기준점이다.
};

// Trail emitter가 attachment owner에게서 sample만 읽기 위한 좁은 provider 계약이다.
class ENGINE_DLL IEffectTrailSampleProvider
{
public:
    virtual ~IEffectTrailSampleProvider() = default;
    virtual bool Try_GetTrailSample(EffectTrailSample& outSample) const = 0;
};

//## Module::Sprite

// Effect runtime이 어떤 기준으로 sprite/mesh facing 축을 고를지 결정한다.
enum class EffectScreenAlignment : uint32
{
    FacingCameraPosition = 0, // particle 위치에서 camera position을 바라보는 기본 billboard다.
    Rectangle = 1,            // 카메라 화면 평면 right/up을 따라가는 직사각형 billboard다.
    Square = 2,               // 카메라 화면 평면 기준에서 더 큰 축으로 정사각형을 만든다.
    AwayFromCenter = 3,       // emitter 중심에서 particle 바깥으로 향하는 radial 방향을 기준 방향으로 쓴다.
    Velocity = 4,             // compute가 기록한 현재 속도 방향을 기준 방향으로 쓴다.
    WorldPlaneXY = 5,         // 월드 XY 평면에 고정되는 2D plane 정렬이다.
    WorldPlaneXZ = 6,         // 월드 XZ 평면에 고정되는 바닥형 2D plane 정렬이다.
};

// AwayFromCenter/Velocity가 기준 방향을 어떻게 쓸지 결정한다.
enum class EffectSpriteDirectionalAlignmentMode : uint32
{
    LookDirection = 0, // 기준 방향을 sprite normal/look 방향으로 사용한다.
    TextureAxis = 1,   // camera-facing 평면을 유지하고 texture 축만 기준 방향에 맞춘다.
};

// TextureAxis mode에서 기준 방향에 맞출 texture 축을 고른다.
enum class EffectSpriteTextureAxis : uint32
{
    X = 0, // texture의 가로 축을 기준 방향에 맞춘다.
    Y = 1, // texture의 세로 축을 기준 방향에 맞춘다.
};

// Material scalar modulation이 조절할 기본 material scalar field다.
enum class EffectMaterialScalarModulationTarget : uint32
{
    Intensity = 0,
    OpacityPower = 1,
    NoiseStrength = 2,
    AlphaErosion = 3,
    AlphaCutoff = 4,
    CoreIntensity = 5,
    OuterIntensity = 6,
    CoreColor = 7,
};

// Material scalar modulation의 base value 적용 방식이다.
enum class EffectMaterialScalarModulationOperation : uint32
{
    Multiply = 0,
};

// Material scalar modulation이 평가할 시간 축이다.
enum class EffectMaterialScalarModulationTimeSource : uint32
{
    ParticleLife = 0,
    EmitterTime = 1,
};

// Material scalar modulation의 float curve payload다.
struct EffectMaterialScalarModulationCurveDesc
{
    bool enabled{ false };
    uint32 keyCount{ 2 };
    Vec4 keyTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 keyTimesBlock1{};
    Vec4 keyValues{ 1.f, 1.f, 0.f, 0.f };
    Vec4 keyValuesBlock1{};
    Vec4 keyArriveTangents{};
    Vec4 keyArriveTangentsBlock1{};
    Vec4 keyLeaveTangents{};
    Vec4 keyLeaveTangentsBlock1{};
    Vec4 keyModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 keyModesBlock1{};
};

struct EffectMaterialScalarModulatorRuntimeDesc
{
    bool enabled{ true };
    EffectMaterialScalarModulationTarget targetField{ EffectMaterialScalarModulationTarget::Intensity };
    EffectMaterialScalarModulationOperation operation{ EffectMaterialScalarModulationOperation::Multiply };
    EffectMaterialScalarModulationTimeSource timeSource{ EffectMaterialScalarModulationTimeSource::ParticleLife };
    EffectMaterialScalarModulationCurveDesc curve{};
};

// Material scalar modulation module에서 shader/renderer가 공유할 compact payload다.
struct EffectMaterialScalarModulationRuntimeDesc
{
    static constexpr uint32 kMaxModulators{ 8 };

    uint32 count{ 0 };
    std::array<EffectMaterialScalarModulatorRuntimeDesc, kMaxModulators> modulators{};
};

// Core color RGB를 emitter time 기준으로 교체하기 위한 Vec3 curve payload다.
struct EffectMaterialCoreColorRgbModulationCurveDesc
{
    bool enabled{ false };
    uint32 keyCount{ 2 };
    Vec4 keyTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 keyTimesBlock1{};
    Vec4 keyValuesR{ 1.f, 1.f, 0.f, 0.f };
    Vec4 keyValuesRBlock1{};
    Vec4 keyValuesG{ 0.85f, 0.85f, 0.f, 0.f };
    Vec4 keyValuesGBlock1{};
    Vec4 keyValuesB{ 0.45f, 0.45f, 0.f, 0.f };
    Vec4 keyValuesBBlock1{};
    Vec4 keyArriveTangentsR{};
    Vec4 keyArriveTangentsRBlock1{};
    Vec4 keyArriveTangentsG{};
    Vec4 keyArriveTangentsGBlock1{};
    Vec4 keyArriveTangentsB{};
    Vec4 keyArriveTangentsBBlock1{};
    Vec4 keyLeaveTangentsR{};
    Vec4 keyLeaveTangentsRBlock1{};
    Vec4 keyLeaveTangentsG{};
    Vec4 keyLeaveTangentsGBlock1{};
    Vec4 keyLeaveTangentsB{};
    Vec4 keyLeaveTangentsBBlock1{};
    Vec4 keyModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 keyModesBlock1{};
};

struct EffectMaterialCoreColorRgbModulatorRuntimeDesc
{
    bool enabled{ true };
    EffectMaterialScalarModulationTimeSource timeSource{ EffectMaterialScalarModulationTimeSource::EmitterTime };
    EffectMaterialCoreColorRgbModulationCurveDesc curve{};
};

// Core color RGB distribution module에서 renderer가 CPU로 평가할 compact payload다.
struct EffectMaterialCoreColorRgbModulationRuntimeDesc
{
    static constexpr uint32 kMaxModulators{ 8 };

    uint32 count{ 0 };
    std::array<EffectMaterialCoreColorRgbModulatorRuntimeDesc, kMaxModulators> modulators{};
};

// Required 모듈에서 material binding에 필요한 값만 접은 runtime payload다.
struct EffectRequiredMaterialRuntimeDesc
{
    EffectMaterialFamily materialFamily{ EffectMaterialFamily::Default }; // runtime material shader/pass family를 고르는 분류다.
    string mainTextureGuid{};                                             // main particle texture asset identity다.
    string mainTexturePath{};                                             // main particle texture resource-relative fallback/debug hint다.
    string noiseTextureGuid{};                                            // (선택) noise texture asset identity다.
    string noiseTexturePath{};                                            // (선택) noise texture resource-relative fallback/debug hint다.
    string maskTextureGuid{};                                             // (선택) mask texture asset identity다.
    string maskTexturePath{};                                             // (선택) mask texture resource-relative fallback/debug hint다.
    string flowTextureGuid{};                                             // SpriteDistortion flow texture asset identity다.
    string flowTexturePath{};                                             // SpriteDistortion flow texture resource-relative fallback/debug hint다.

    EffectMaterialBlendMode blendMode{ EffectMaterialBlendMode::AlphaBlend }; // effect material blend/compose mode다.
    string opacitySource{ "Alpha" };                                          // opacity source selector다.
    Vec4 tint{ 1.f, 1.f, 1.f, 1.f };                                          // texture sample에 곱할 tint다.
    float intensity{ 1.f };                                                   // sprite 색상 밝기/발광감 계수다.
    float opacityPower{ 1.f };                                                // opacity sharpening 계수다.
    float noiseStrength{ 0.f };                                               // noise modulation 강도다.
    float alphaCutoff{ 0.f };                                                 // Trail 등 지원 renderer가 사용할 alpha discard threshold다.
    float alphaErosion{ 0.f };                                                // mask/noise 기반 alpha erosion 후보 값이다.
    string noiseSource{ "Red" };                                              // noise texture scalar source selector다.
    string maskSource{ "Alpha" };                                             // mask texture coverage source selector다.
    bool noiseInvert{ false };                                                // noise scalar를 1-x로 뒤집는다.
    bool maskInvert{ false };                                                 // mask coverage를 1-x로 뒤집는다.

    Vec2 mainUVScale{ 1.f, 1.f }; // main texture UV 반복/확대 배율 후보 값이다.
    Vec2 mainUVOffset{ 0.f, 0.f }; // main texture 샘플 시작 위치를 정적으로 민다.
    Vec2 mainUVScrollSpeed{ 0.f, 0.f }; // main texture 샘플 좌표를 시간에 따라 밀어 흐름감을 만드는 속도 후보 값이다.
    EffectTextureUVTilingMode mainUVTilingMode{ EffectTextureUVTilingMode::Wrap }; // main texture UV tiling 해석 방식이다.
    EffectMaterialUVAxisPolicy mainUVPolicy{ EffectTextureUVTilingMode::Wrap, EffectTextureUVTilingMode::Wrap }; // main texture U/V축별 UV 해석 방식이다.

    Vec2 noiseUVScale{ 1.f, 1.f }; // noise texture UV 반복/확대 배율 후보 값이다.
    Vec2 noiseUVOffset{ 0.f, 0.f }; // noise texture 샘플 시작 위치를 정적으로 민다.
    Vec2 noiseUVScrollSpeed{ 0.f, 0.f }; // noise를 frame 교체가 아니라 UV 좌표 이동으로 흘려 불규칙한 modulation 흐름을 만드는 속도 후보 값이다.
    EffectTextureUVTilingMode noiseUVTilingMode{ EffectTextureUVTilingMode::Wrap }; // noise texture UV tiling 해석 방식이다.
    EffectMaterialUVAxisPolicy noiseUVPolicy{ EffectTextureUVTilingMode::Wrap, EffectTextureUVTilingMode::Wrap }; // noise texture U/V축별 UV 해석 방식이다.

    Vec2 maskUVScale{ 1.f, 1.f }; // mask texture UV 반복/확대 배율 후보 값이다.
    Vec2 maskUVOffset{ 0.f, 0.f }; // mask texture 샘플 시작 위치를 정적으로 민다.
    Vec2 maskUVScrollSpeed{ 0.f, 0.f }; // mask를 frame 교체가 아니라 UV 좌표 이동으로 흘려 alpha/erosion 변화를 만드는 속도 후보 값이다.
    EffectTextureUVTilingMode maskUVTilingMode{ EffectTextureUVTilingMode::Wrap }; // mask texture UV tiling 해석 방식이다.
    EffectMaterialUVAxisPolicy maskUVPolicy{ EffectTextureUVTilingMode::Wrap, EffectTextureUVTilingMode::Wrap }; // mask texture U/V축별 UV 해석 방식이다.

    Vec2 flowUVScale{ 1.f, 1.f }; // SpriteDistortion flow texture UV 반복/확대 배율 후보 값이다.
    Vec2 flowUVOffset{ 0.f, 0.f }; // SpriteDistortion flow texture 샘플 시작 위치를 정적으로 민다.
    Vec2 flowUVScrollSpeed{ 0.f, 0.f }; // SpriteDistortion flow texture UV scroll speed 후보 값이다.
    EffectTextureUVTilingMode flowUVTilingMode{ EffectTextureUVTilingMode::Wrap }; // flow texture UV tiling 해석 방식이다.
    EffectMaterialUVAxisPolicy flowUVPolicy{ EffectTextureUVTilingMode::Wrap, EffectTextureUVTilingMode::Wrap }; // flow texture U/V축별 UV 해석 방식이다.

    EffectMaterialAdditiveContributionData additive{};                                // Additive blend에서 더할 contribution 해석 규칙이다.
    EffectMaterialCoreEmissiveData coreEmissive{};                                    // alpha-derived inner core/outer halo shaping 값이다.
    float refractionIntensity{ 0.f };                                                 // SpriteDistortion screen UV offset 강도다.
    float refractionPresence{ 1.f };                                                  // SpriteDistortion refraction 영향도 scalar다.
    EffectDistortionShapeMode distortionShapeMode{ EffectDistortionShapeMode::None }; // SpriteDistortion 영향 범위 weight mode다.
    float distortionShapeRadius{ 0.45f };                                             // sprite UV 중심에서 shape 기준 반경이다.
    float distortionShapeThickness{ 0.10f };                                          // Ring은 UV 폭, ShockwaveRing은 screen pixel half-width다.
    float distortionShapeSoftness{ 0.15f };                                           // Ring은 UV falloff, ShockwaveRing은 screen pixel falloff다.
    bool twoSided{ false };                                                           // 지원 renderer가 사용할 양면 렌더 힌트다.
    uint32 subUVRows{ 1 };                                                            // flipbook row 수다.
    uint32 subUVCols{ 1 };                                                            // flipbook column 수다.
    EffectMaterialCoreColorRgbModulationRuntimeDesc coreColorRgbModulation{};         // coreColor.rgb emitter-time distribution payload다.
    EffectMaterialScalarModulationRuntimeDesc scalarModulation{};                     // material scalar modulation compact payload다.
};

// Required 모듈에서 sprite render alignment에 필요한 값만 접은 runtime payload다.
struct EffectSpriteRenderRuntimeDesc
{
    EffectScreenAlignment screenAlignment{ EffectScreenAlignment::FacingCameraPosition }; // sprite 축 선택 기준이다.
    EffectSpriteDirectionalAlignmentMode directionalAlignmentMode{ EffectSpriteDirectionalAlignmentMode::TextureAxis }; // 방향 기준 사용 방식이다.
    EffectSpriteTextureAxis spriteTextureAxis{ EffectSpriteTextureAxis::Y }; // TextureAxis mode에서 맞출 texture 축이다.
    float spriteRollOffsetDegrees{ 0.f }; // authored texture 방향 차이를 보정하는 정적 roll offset이다.
};

// Required 모듈에서 emitter playback에 필요한 값만 접은 runtime payload다.
struct EffectEmitterPlaybackRuntimeDesc
{
    float duration{ 1.f };            // emitter 한 loop가 재생되는 시간이다.
    uint32 loopCount{ 0 };            // 0은 무한 반복, 1 이상은 지정 횟수 재생이다.
    float delay{ 0.f };               // loop 시작 전 spawn/draw를 막는 지연 시간이다.
    bool delayFirstLoopOnly{ false }; // true면 첫 loop에만 delay를 적용한다.
    bool killOnDeactivate{ false };   // 외부 deactivate 계약이 생기면 active particle을 즉시 죽일지 나타낸다.
    bool killOnCompleted{ false };    // 마지막 loop 완료 시 active particle을 즉시 죽일지 나타낸다.
};

// Required 모듈에서 draw 수 제한에 필요한 값만 접은 runtime payload다.
struct EffectDrawLimitRuntimeDesc
{
    bool useMaxDrawCount{ true }; // maxDrawCount를 draw 상한으로 적용할지 나타낸다.
    uint32 maxDrawCount{ 500 };   // 동시에 그릴 particle 수 상한이다. buffer capacity는 바꾸지 않는다.
};

// Required 모듈에서 Blend 정렬 보정에 필요한 값만 접은 runtime payload다.
struct EffectRequiredSortRuntimeDesc
{
    EffectSortPolicy sortPolicy{ EffectSortPolicy::None }; // 같은 sort layer 안에서 어떤 Blend 정렬을 적용할지 결정한다.
    int32 sortLayer{ 0 };                                  // 낮은 layer를 먼저 그리고 높은 layer를 나중에 그린다.
    float artistSortBias{ 0.f };                           // 양수는 더 앞, 음수는 더 뒤로 보정하려는 제작자 의도 값이다.
};

// Required 모듈의 runtime 소비 payload를 의미별로 묶은 desc다.
struct EffectRequiredRuntimeDesc
{
    EffectRequiredMaterialRuntimeDesc material{};
    EffectSpriteRenderRuntimeDesc spriteRender{};
    EffectEmitterPlaybackRuntimeDesc playback{};
    EffectDrawLimitRuntimeDesc drawLimit{};
    EffectRequiredSortRuntimeDesc sort{};
};

// Spawn 모듈의 runtime 소비 payload와 spawn fallback shape를 묶은 desc다.
struct EffectSpawnRuntimeDesc
{
    uint32 instanceCount{ 96 };             // 생성할 particle 수다.
    Vec3 spawnRange{ 1.f, 1.f, 1.f };       // Phase2 spawn shape 전까지 사용할 기본 box spawn 범위다.
    PointParticleSpawnDesc particleSpawn{}; // Spawn 모듈이 만든 연속/버스트 생성 규칙이다.
};

// Lifetime 모듈에서 compute payload에 필요한 값만 접은 runtime payload다.
struct EffectLifetimeRuntimeDesc
{
    Vec2 lifeTime{ 0.5f, 1.f }; // compute payload가 사용할 생존 시간 범위다.
};

// InitialSize 모듈에서 compute payload에 필요한 값만 접은 runtime payload다.
struct EffectInitialSizeRuntimeDesc
{
    bool enabled{ false };        // InitialSize 모듈이 runtime에 연결되었는지 나타낸다.
    Vec2 sizeMin{ 0.12f, 0.12f }; // InitialSize의 가로/세로 크기 하한이다.
    Vec2 sizeMax{ 0.28f, 0.28f }; // InitialSize의 가로/세로 크기 상한이다.
    PointParticleRandomSeedRuntimeDesc sizeSeed{};
};

// InitialColor 모듈에서 compute payload에 필요한 값만 접은 runtime payload다.
struct EffectInitialColorRuntimeDesc
{
    Vec4 startColorMin{ 1.f, 1.f, 1.f, 1.f }; // Initial Color 하한이다.
    Vec4 startColorMax{ 1.f, 1.f, 1.f, 1.f }; // Initial Color 상한이다.
    PointParticleRandomSeedRuntimeDesc colorSeed{};
    PointParticleRandomSeedRuntimeDesc alphaSeed{};
};

// ColorOverLife 모듈에서 compute payload에 필요한 값만 접은 runtime payload다.
struct EffectColorOverLifeRuntimeDesc
{
    Vec4 endColorMin{ 1.f, 1.f, 1.f, 1.f };      // Color Over Life 하한이다.
    Vec4 endColorMax{ 1.f, 1.f, 1.f, 1.f };      // Color Over Life 상한이다.
    PointParticleColorOverLifeCurveDesc curve{}; // ConstantCurve ColorOverLife runtime payload다.
    PointParticleRandomSeedRuntimeDesc colorSeed{};
    PointParticleRandomSeedRuntimeDesc alphaSeed{};
};

// Trail 전용 SpawnPerUnit 모듈에서 이동 거리 기반 보강 sample 생성에 필요한 값만 접은 payload다.
struct EffectTrailSpawnPerUnitRuntimeDesc
{
    bool enabled{ false };          // true면 이동 거리 기준으로 history sample을 보강 삽입한다.
    float spawnPerUnit{ 0.f };      // unitScalar 기준 단위 거리당 생성할 sample 수다.
    float unitScalar{ 1.f };        // spawnPerUnit을 실제 world distance로 환산하는 스칼라다.
    float movementTolerance{ 0.f }; // 이 거리 미만 이동은 새 sample을 만들지 않는다.
    float maxFrameDistance{ 0.f };  // 이 거리보다 큰 frame 이동은 teleport/skip으로 보고 history를 재시드한다. 0 이하면 제한하지 않는다.
};

//## Emitter

// compute sprite emitter가 실제 runtime consumer에게 넘길 module-facing payload다.
struct ComputeSpriteEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    PointParticleDrawMode drawMode{ PointParticleDrawMode::DrawIndexedInstancedIndirect };          // compute sprite 정식 draw 제출 방식이다.
    EffectEmitterRenderLayerOverride renderLayerOverride{ EffectEmitterRenderLayerOverride::Auto }; // Auto면 material/type 기반 render group을 사용한다.

    EffectRequiredRuntimeDesc required{};                             // Required 모듈에서 runtime이 소비하는 값이다.
    EffectSpawnRuntimeDesc spawn{};                                   // Spawn 모듈에서 runtime이 소비하는 값이다.
    EffectLifetimeRuntimeDesc lifetime{};                             // Lifetime 모듈에서 runtime이 소비하는 값이다.
    EffectInitialSizeRuntimeDesc initialSize{};                       // InitialSize 모듈에서 runtime이 소비하는 값이다.
    EffectInitialColorRuntimeDesc initialColor{};                     // InitialColor 모듈에서 runtime이 소비하는 값이다.
    EffectColorOverLifeRuntimeDesc colorOverLife{};                   // ColorOverLife 모듈에서 runtime이 소비하는 값이다.
    PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{};         // 수명 진행률 기준 SubUV frame 규칙이다.
    PointParticleSizeByLifeDesc sizeByLife{};                         // 수명 진행률 기준 크기 배율 규칙이다.
    PointParticleMotionDesc motion{};                                 // motion authoring module을 compute payload로 압축한 규칙이다.
    EffectOrbitOverLifeRuntimeDesc orbitOverLife{};                   // OrbitOverLife 모듈이 만든 emitter-local pivot 회전 payload다.
    PointParticleRotationDesc rotation{};                             // rotation authoring module을 compute payload로 압축한 규칙이다.
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};                // Lifetime random sample seed salt다.
    PointParticleInitialLocationDesc initialLocation{};               // InitialLocation 모듈이 만든 시작 위치 offset 분포다.
    PointParticleSphereLocationDesc sphereLocation{};                 // SphereLocation 모듈이 만든 시작 위치 구형 offset 분포다.
    PointParticlePlaneRadialLocationDesc planeRadialLocation{};       // PlaneRadialLocation 모듈이 만든 시작 위치 평면/방사 offset 분포다.
    PointParticlePlaneRadialOrientationDesc planeRadialOrientation{}; // PlaneRadialOrientation 모듈이 만든 radial frame 기반 초기 회전 payload다.
    PointParticleRandomSeedRuntimeDesc initialLocationSeed{};         // InitialLocation random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc sphereLocationSeed{};          // SphereLocation random sample seed salt다.
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{};        // SubUV RandomFrame random sample seed salt다.
    Vec3 center{ 0.f, 0.f, 0.f };                                     // concrete emitter 내부 fallback/초기 center다. root transform이 있으면 후속 sync가 덮어쓴다.
};

// compute trail emitter가 clone 시점에 받는 trail 전용 runtime payload다.
struct ComputeTrailEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{ EffectEmitterRenderLayerOverride::Auto }; // Auto면 material family 기반 render group을 사용한다.
    EffectRequiredMaterialRuntimeDesc material{};                                                   // trail 기본 material/tint/blend 값이다.
    EffectEmitterPlaybackRuntimeDesc playback{};                                                    // duration/loop/delay 같은 공통 재생 규칙이다.
    EffectLifetimeRuntimeDesc lifetime{};                                                           // Lifetime 모듈에서 trail이 소비하는 age fade 기준 값이다.
    EffectInitialColorRuntimeDesc initialColor{};                                                   // InitialColor 모듈에서 trail segment 시작 색상으로 소비하는 값이다.
    EffectColorOverLifeRuntimeDesc colorOverLife{};                                                 // ColorOverLife 모듈에서 trail segment 끝 색상으로 소비하는 값이다.
    PointParticleSizeByLifeDesc sizeByLife{};                                                       // SizeByLife 모듈에서 trail 폭 배율로 소비하는 값이다.
    EffectTrailSpawnPerUnitRuntimeDesc spawnPerUnit{};                                              // SpawnPerUnit 모듈에서 trail sample density를 보강하는 값이다.

    float width{ 1.f };             // render sample의 base/tip span에 곱할 폭 비율이다. 1이면 원본 base/tip 폭을 유지한다.
    float segmentLifetime{ 0.25f }; // history sample 하나가 화면에 남는 시간이다.
    uint32 historyCount{ 32 };      // trail이 유지할 최대 sample 개수다.
    float uvTiling{ 1.f };          // trail noise/breakup 진행 방향의 기본 UV 반복값이다.
    float maxTrailLength{ 0.f };    // head 기준 최대 표시 길이다. 0 이하면 제한하지 않는다.
    float tailFadeLength{ 0.1f };   // maxTrailLength 끝으로 갈수록 alpha를 줄이는 거리다.
    bool autoLifeFade{ true };      // sample age 기준 자동 alpha fade를 적용할지 결정한다.
    float sampleSpacing{ 0.08f };   // render-time curve sample의 이동 거리 기준 간격이다.
    uint32 curveSubdivision{ 4 };   // 원본 sample interval 하나에서 만들 최대 render segment 수다.
    bool smoothTangent{ true };     // render sample 생성 전 원본 history tangent를 완만하게 보정할지 결정한다.
    float sideFade{ 0.08f };        // 폭 방향 alpha edge 감소 강도다.

    Weak<IEffectTrailSampleProvider> sampleProvider{}; // attachment 계산 owner가 공급하는 sample source다.
};

enum class EffectSourceHistoryPreviewMotionPreset : uint8
{
    Off,
    Line,
    Arc,
    Loop,
};

enum class EffectSourceHistoryPreviewMotionAxis : uint8
{
    X,
    Y,
    Z,
};

enum class EffectRibbonRenderAxis : uint8
{
    CameraUp,
    ViewUp,
    WorldUp,
};

// source/root position history를 camera-facing strip으로 낮추는 runtime payload다.
struct ComputeSourceHistoryRibbonEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{ EffectEmitterRenderLayerOverride::Auto }; // Auto면 material family 기반 render group을 사용한다.
    EffectRequiredMaterialRuntimeDesc material{};                                                   // ribbon 기본 material/tint/blend 값이다.
    EffectEmitterPlaybackRuntimeDesc playback{};                                                    // duration/loop/delay 같은 공통 재생 규칙이다.
    EffectLifetimeRuntimeDesc lifetime{};                                                           // Lifetime 모듈에서 sample age 기준 fallback으로 소비하는 값이다.
    EffectRequiredSortRuntimeDesc sort{};                                                           // Required 모듈에서 Blend 정렬 규칙으로 소비하는 값이다.
    EffectInitialColorRuntimeDesc initialColor{};                                                   // InitialColor 모듈에서 source history sample 시작 색으로 소비하는 값이다.
    EffectColorOverLifeRuntimeDesc colorOverLife{};                                                 // ColorOverLife 모듈에서 sample age 기준 색/알파로 소비하는 값이다.
    PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{};                                       // 수명 진행률 기준 SubUV frame 규칙이다.
    PointParticleSizeByLifeDesc sizeByLife{};                                                       // SizeByLife 모듈에서 source history strip 폭 배율로 소비하는 값이다.
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{};                                      // SubUV RandomFrame replay 단위 seed salt다.

    uint32 maxSampleCount{ 64 }; // source history가 보관할 최대 sample 개수다.
    float sampleSpacing{ 0.08f }; // source가 이 거리 이상 움직였을 때 새 sample을 추가한다.
    float sampleInterval{ 0.f }; // 0보다 크면 정지/저속 source도 이 시간마다 sample을 보강한다.
    float sampleLifetime{ 0.35f }; // sample 하나가 strip 구성 후보로 남는 시간이다.
    float maxLength{ 0.f }; // head 기준 표시 가능한 최대 길이다. 0 이하면 제한하지 않는다.
    float tailFadeLength{ 0.f }; // maxLength 끝으로 갈수록 alpha를 줄이는 거리다. 0 이하면 끈다.
    float baseWidth{ 0.3f }; // SizeByLife를 곱하기 전 source history strip 기본 폭이다.
    float tilingDistance{ 0.f }; // 0 이하면 visible length stretch, 양수면 거리 기준 texture 반복이다.
    bool autoLifeFade{ false }; // sample age 기준 자동 alpha fade를 적용할지 결정한다.
    EffectSourceHistoryPreviewMotionPreset previewMotionPreset{ EffectSourceHistoryPreviewMotionPreset::Line }; // EffectEditor 검증용 source motion preset이다.
    EffectSourceHistoryPreviewMotionAxis previewMotionAxis{ EffectSourceHistoryPreviewMotionAxis::X }; // preview motion 진행 축이다.
    float previewMotionAmplitude{ 2.f }; // preview motion offset 크기다.
    float previewMotionSpeed{ 3.f }; // one-shot 진행률 또는 loop 반복 속도다.
};

// mesh emitter가 Vec3 transform curve를 CPU instance update에서 평가하기 위한 payload다.
struct MeshVector3CurveRuntimeDesc
{
    bool enabled{ false };                      // true면 lifeProgress 기준 curve를 평가한다.
    Vec3 start{ 1.f, 1.f, 1.f };                // distribution/legacy fallback 시작값이다.
    Vec3 end{ 1.f, 1.f, 1.f };                  // distribution/legacy fallback 종료값이다.
    uint32 curveKeyCount{ 2 };                  // compact curve key 수다.
    Vec4 curveKeyTimes{ 0.f, 1.f, 0.f, 0.f };   // key time 0..1 값이다.
    Vec4 curveKeyTimesBlock1{};                 // key 4..7 time 값이다.
    Vec4 curveKeyValuesX{ 1.f, 1.f, 0.f, 0.f }; // key별 X 값이다.
    Vec4 curveKeyValuesXBlock1{};               // key 4..7 X 값이다.
    Vec4 curveKeyValuesY{ 1.f, 1.f, 0.f, 0.f }; // key별 Y 값이다.
    Vec4 curveKeyValuesYBlock1{};               // key 4..7 Y 값이다.
    Vec4 curveKeyValuesZ{ 1.f, 1.f, 0.f, 0.f }; // key별 Z 값이다.
    Vec4 curveKeyValuesZBlock1{};               // key 4..7 Z 값이다.
};

// mesh emitter 전용 Vec3 scale/rotation runtime payload다.
struct MeshTransformRuntimeDesc
{
    EffectScreenAlignment alignment{ EffectScreenAlignment::FacingCameraPosition }; // mesh instance의 기본 plane/facing 정렬 기준이다.

    bool initialScaleEnabled{ false };           // InitialMeshSize Vec3 payload 연결 여부다.
    Vec3 initialScaleMin{ 0.12f, 0.12f, 0.12f }; // spawn 시 mesh scale 하한이다.
    Vec3 initialScaleMax{ 0.28f, 0.28f, 0.28f }; // spawn 시 mesh scale 상한이다.
    MeshVector3CurveRuntimeDesc scaleByLife{};   // MeshSizeByLife Vec3 multiplier다.

    bool rotationEnabled{ false };                            // rotation 계열 모듈 연결 여부다.
    Vec3 initialRotationDegreesMin{ 0.f, 0.f, 0.f };          // spawn 시 Euler rotation 하한이다.
    Vec3 initialRotationDegreesMax{ 0.f, 0.f, 0.f };          // spawn 시 Euler rotation 상한이다.
    MeshVector3CurveRuntimeDesc rotationByLife{};             // lifeProgress 기준 추가 Euler degree다.
    Vec3 initialAngularVelocityDegreesMin{ 0.f, 0.f, 0.f };   // 초당 Euler angular velocity 하한이다.
    Vec3 initialAngularVelocityDegreesMax{ 0.f, 0.f, 0.f };   // 초당 Euler angular velocity 상한이다.
    MeshVector3CurveRuntimeDesc angularVelocityScaleByLife{}; // angular velocity Vec3 배율이다.
};

// mesh emitter가 clone 시점에 받는 model/instance draw용 runtime payload다.
struct MeshEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto
    };                                                                  // Auto면 material/blend mode 기반 render group을 사용한다.
    string modelGuid{};                                                 // 선택 model asset identity다.
    string modelPath{};                                                 // asset-root 기준 model path fallback/debug hint다.
    Vec3 previewScale{ 1.f, 1.f, 1.f };                                 // Phase 3 단일 mesh particle preview scale이다.
    bool useModelMaterials{ false };                                    // true면 source model material을 우선 사용한다.
    EffectRequiredMaterialRuntimeDesc material{};                       // Phase 3에서는 tint/debug payload로 보존하고 material override는 후속에서 소비한다.
    EffectEmitterPlaybackRuntimeDesc playback{};                        // Required 모듈에서 mesh emitter가 소비하는 재생 규칙이다.
    EffectDrawLimitRuntimeDesc drawLimit{};                             // Required 모듈에서 mesh draw 제출 상한으로 소비하는 규칙이다.
    EffectRequiredSortRuntimeDesc sort{};                               // Required 모듈에서 mesh emitter가 소비하는 Blend 정렬 규칙이다.
    EffectSpawnRuntimeDesc spawn{};                                     // Spawn 모듈에서 mesh particle 활성화 규칙으로 소비하는 payload다.
    EffectLifetimeRuntimeDesc lifetime{};                               // Lifetime 모듈에서 mesh particle age/max life로 소비하는 payload다.
    EffectInitialColorRuntimeDesc initialColor{};                       // InitialColor 모듈에서 mesh particle 시작 색으로 소비하는 payload다.
    EffectColorOverLifeRuntimeDesc colorOverLife{};                     // ColorOverLife 모듈에서 mesh particle color/fade로 소비하는 payload다.
    PointParticleMotionDesc motion{};                                   // InitialVelocity/RadialVelocity 모듈에서 mesh particle 위치 갱신으로 소비하는 payload다.
    EffectOrbitOverLifeRuntimeDesc orbitOverLife{};                     // OrbitOverLife 모듈에서 mesh particle center 위치 공전으로 소비하는 payload다.
    PointParticleInitialLocationDesc initialLocation{};                 // InitialLocation 모듈에서 mesh spawn offset으로 소비하는 payload다.
    PointParticleSphereLocationDesc sphereLocation{};                   // SphereLocation 모듈에서 mesh spawn offset으로 소비하는 payload다.
    PointParticleRandomSeedRuntimeDesc sphereLocationSeed{};            // SphereLocation random sample seed salt다.
    PointParticlePlaneRadialLocationDesc planeRadialLocation{};         // PlaneRadialLocation 모듈에서 mesh spawn offset으로 소비하는 payload다.
    PointParticleSphereRadialOrientationDesc sphereRadialOrientation{}; // SphereRadialOrientation 모듈에서 sphere radial frame 회전으로 소비하는 payload다.
    PointParticlePlaneRadialOrientationDesc planeRadialOrientation{};   // PlaneRadialOrientation 모듈에서 mesh radial frame 회전으로 소비하는 payload다.
    MeshTransformRuntimeDesc meshTransform{};                           // Mesh 전용 Vec3 scale/rotation payload다.
};

using EffectEmitterConcreteDesc =
variant<ComputeSpriteEmitterDesc, ComputeTrailEmitterDesc, ComputeSourceHistoryRibbonEmitterDesc, MeshEmitterDesc>;

struct EffectEmitterDefinition
{
    uint32 id{};                                                // authoring/runtime 추적용 emitter id다.
    string name{};                                              // 디버그/로그용 emitter 이름이다.
    EffectEmitterKind kind{ EffectEmitterKind::ComputeSprite }; // 생성할 concrete emitter 종류다.
    bool enabled{ true };                                       // root가 runtime child 생성 대상으로 볼지 결정하는 플래그다.

    Vec3 localPosition{ 0.f, 0.f, 0.f };                                                            // effect root 기준 emitter local 위치다.
    Vec3 localRotationDegrees{ 0.f, 0.f, 0.f };                                                     // effect root 기준 emitter local 회전이다.
    bool useLocalSpace{ false };                                                                    // true면 root transform을 따라가는 local-space emitter다.
    EffectEmitterRenderLayerOverride renderLayerOverride{ EffectEmitterRenderLayerOverride::Auto }; // Auto면 concrete emitter의 기존 자동 render group 정책을 따른다.

    EffectEmitterConcreteDesc concreteDesc{ ComputeSpriteEmitterDesc{} }; // concrete emitter가 직접 소비할 payload다.
};

//## Effect

// 저장/로드 결과로 공유되는 effect 정적 정의다.
struct EffectDefinition
{
    string name{};                              // effect asset/display 이름이다.
    vector<EffectEmitterDefinition> emitters{}; // effect를 구성하는 emitter definition 목록이다.
};

// EffectInstance root spawn 시 전달하는 runtime desc다.
struct EffectInstanceDesc : public GameObject::GAMEOBJECT_DESC
{
    Shared<const EffectDefinition> definition{}; // root가 재생할 정적 effect 정의다.
};

NS_END
