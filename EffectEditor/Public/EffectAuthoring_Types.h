#pragma once
#include <variant>
#include "EffectMaterialAuthoring_Types.h"
#include "Particle_Types.h"

NS_BEGIN(EffectEditor)

enum class AuthoringModuleType : uint8
{
    Required,
    Spawn,
    Lifetime,
    InitialLocation,
    SphereLocation,
    InitialSize,
    InitialVelocity,
    InitialRadialVelocity,
    Acceleration,
    Drag,
    VelocityOverLife,
    InitialRotation,
    RotationOverLife,
    InitialRotationRate,
    RotationRateScaleByLife,
    InitialColor,
    ColorOverLife,
    SubUVFrameOverLife,
    SizeByLife,
    SpawnPerUnit,
};

enum class EffectAuthoringSelectionKind : uint8
{
    None,
    Emitter,
    TypeData,
    Module,
};

enum class EmitterScreenAlignment : uint8
{
    Square,
    FacingCameraPosition,
    Rectangle,
    Velocity,
    AwayFromCenter,
    TypeSpecific,
    FacingCameraDistanceBlend,
};

// AwayFromCenter/Velocity가 기준 방향을 어떻게 사용할지 정한다.
enum class EmitterDirectionalAlignmentMode : uint8
{
    LookDirection, // 기준 방향을 sprite 카드의 바라보는 방향으로 사용한다.
    TextureAxis,   // camera-facing 평면은 유지하고 texture 축만 기준 방향에 맞춘다.
};

// TextureAxis mode에서 기준 방향에 맞출 texture 축을 고른다.
enum class EmitterSpriteTextureAxis : uint8
{
    X, // texture의 가로 축을 radial/velocity 방향에 맞춘다.
    Y, // texture의 세로 축을 radial/velocity 방향에 맞춘다.
};

enum class EmitterSortMode : uint8
{
    None,
    ViewProjDepth,
    DistanceToView,
    AgeOldestFirst,
    AgeNewestFirst,
};

enum class DistributionValueKind : uint8
{
    Float,
    Vector2,
    Vector3,
    Color,
};

enum class DistributionMode : uint8
{
    Constant,
    Uniform,
    ConstantCurve,
    UniformCurve,
    ParticleParameter,
};

// 현재 authoring payload가 실제로 보유하는 분포 모드만 true로 둔다.
constexpr bool Is_DistributionModeEditable(DistributionMode mode)
{
    return mode == DistributionMode::Constant ||
           mode == DistributionMode::Uniform ||
           mode == DistributionMode::ConstantCurve;
}

// 분포 플로트에서의 보간 모드 설정이다.
enum class FloatCurveInterpolationMode : uint8
{
    Linear,           // key 사이를 직선으로 잇는 기본 모드다.
    Constant,         // 다음 key 전까지 값을 유지하는 계단형 모드다.
    CurveAuto,        // 후속 지원 예정인 자동 tangent curve 모드다.
    CurveUser,        // 후속 지원 예정인 수동 tangent curve 모드다.
    CurveBreak,       // 후속 지원 예정인 좌우 tangent 분리 모드다.
    CurveAutoClamped, // v1에서 실제 지원할 안전한 자동 curve 모드다.
};

// 분포 플로트 상수 커브의 필요 값들이다.
struct FloatCurveKeyData
{
    float time{ 0.f };          // 0..1 공통축에서 이 key의 위치다.
    float value{ 0.f };         // 이 key 시점의 출력값이다.
    float arriveTangent{ 0.f }; // key로 들어오는 곡선 기울기다.
    float leaveTangent{ 0.f };  // key에서 나가는 곡선 기울기다.
    FloatCurveInterpolationMode interpolationMode{
        FloatCurveInterpolationMode::Linear
    }; // 이 key 이후 구간의 보간 방식을 결정한다.
};

// 분포 플로트 상수 커브의 보간 key 목록을 보관한다.
struct ConstantCurveFloatDistributionData
{
    // 분포 플로트 상수 커브의 authoring key 목록이다.
    vector<FloatCurveKeyData> keys{
        FloatCurveKeyData{ 0.f, 0.f, 0.f, 0.f, FloatCurveInterpolationMode::Linear },
        FloatCurveKeyData{ 1.f, 0.f, 0.f, 0.f, FloatCurveInterpolationMode::Linear }
    };
};

struct ConstantFloatDistributionData
{
    float value{ 0.f };
};

struct UniformFloatDistributionData
{
    float minValue{ 0.f };
    float maxValue{ 0.f };
};

struct FloatDistributionData
{
    using Payload = variant<
        ConstantFloatDistributionData,
        UniformFloatDistributionData,
        ConstantCurveFloatDistributionData>;

    DistributionMode mode{ DistributionMode::Constant };
    Payload payload{ ConstantFloatDistributionData{} };

    static FloatDistributionData Make_Constant(float value)
    {
        FloatDistributionData data{};
        data.mode = DistributionMode::Constant;
        data.payload = ConstantFloatDistributionData{ value };
        return data;
    }

    static FloatDistributionData Make_Uniform(float minValue, float maxValue)
    {
        FloatDistributionData data{};
        data.mode = DistributionMode::Uniform;
        data.payload = UniformFloatDistributionData{ minValue, maxValue };
        return data;
    }

    static FloatDistributionData Make_ConstantCurve(float value)
    {
        FloatDistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveFloatDistributionData{
            {
                FloatCurveKeyData{ 0.f, value, 0.f, 0.f, FloatCurveInterpolationMode::Linear },
                FloatCurveKeyData{ 1.f, value, 0.f, 0.f, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }
};

struct ConstantVector2DistributionData
{
    Vec2 value{ 0.f, 0.f };
};

struct UniformVector2DistributionData
{
    Vec2 minValue{ 0.f, 0.f };
    Vec2 maxValue{ 0.f, 0.f };
};

struct Vector2DistributionData
{
    using Payload = variant<ConstantVector2DistributionData, UniformVector2DistributionData>;

    DistributionMode mode{ DistributionMode::Constant };
    Payload payload{ ConstantVector2DistributionData{} };

    static Vector2DistributionData Make_Constant(const Vec2& value)
    {
        Vector2DistributionData data{};
        data.mode = DistributionMode::Constant;
        data.payload = ConstantVector2DistributionData{ value };
        return data;
    }

    static Vector2DistributionData Make_Uniform(const Vec2& minValue, const Vec2& maxValue)
    {
        Vector2DistributionData data{};
        data.mode = DistributionMode::Uniform;
        data.payload = UniformVector2DistributionData{ minValue, maxValue };
        return data;
    }
};

struct ConstantVector3DistributionData
{
    Vec3 value{ 0.f, 0.f, 0.f };
};

struct UniformVector3DistributionData
{
    Vec3 minValue{ 0.f, 0.f, 0.f };
    Vec3 maxValue{ 0.f, 0.f, 0.f };
};

struct Vector3DistributionData
{
    using Payload = variant<ConstantVector3DistributionData, UniformVector3DistributionData>;

    DistributionMode mode{ DistributionMode::Constant };
    Payload payload{ ConstantVector3DistributionData{} };

    static Vector3DistributionData Make_Constant(const Vec3& value)
    {
        Vector3DistributionData data{};
        data.mode = DistributionMode::Constant;
        data.payload = ConstantVector3DistributionData{ value };
        return data;
    }

    static Vector3DistributionData Make_Uniform(const Vec3& minValue, const Vec3& maxValue)
    {
        Vector3DistributionData data{};
        data.mode = DistributionMode::Uniform;
        data.payload = UniformVector3DistributionData{ minValue, maxValue };
        return data;
    }
};

struct ConstantColorDistributionData
{
    Color value{ 1.f, 1.f, 1.f, 1.f };
};

struct UniformColorDistributionData
{
    Color minValue{ 1.f, 1.f, 1.f, 1.f };
    Color maxValue{ 1.f, 1.f, 1.f, 1.f };
};

struct ColorDistributionData
{
    using Payload = variant<ConstantColorDistributionData, UniformColorDistributionData>;

    DistributionMode mode{ DistributionMode::Constant };
    Payload payload{ ConstantColorDistributionData{} };

    static ColorDistributionData Make_Constant(const Color& value)
    {
        ColorDistributionData data{};
        data.mode = DistributionMode::Constant;
        data.payload = ConstantColorDistributionData{ value };
        return data;
    }

    static ColorDistributionData Make_Uniform(const Color& minValue, const Color& maxValue)
    {
        ColorDistributionData data{};
        data.mode = DistributionMode::Uniform;
        data.payload = UniformColorDistributionData{ minValue, maxValue };
        return data;
    }
};

struct ParticleSystemAuthoringData
{
    bool autoDeactivate{ true };
};

enum class AuthoringTypeDataKind : uint8
{
    None,
    Trail,
};

enum class TrailCurveQuality : uint8
{
    Basic,       // 기존 호환에 가까운 기본 trail 곡선 preset이다.
    Smooth,      // 보간 밀도를 높여 swing arc를 부드럽게 만드는 preset이다.
    HighQuality, // 더 촘촘한 sample로 곡선을 만드는 고품질 preset이다.
    Custom,      // 세부 곡선 값을 직접 편집하는 모드다.
};

struct EmptyTypeData
{};

struct TrailTypeData
{
    float width{ 1.f };
    float segmentLifetime{ 0.25f };
    uint32 historyCount{ 32 };
    float uvTiling{ 1.f };
    float maxTrailLength{ 0.f };                                // 0 이하면 head 기준 최대 표시 길이를 제한하지 않는다.
    float tailFadeLength{ 0.1f };                               // maxTrailLength 끝으로 갈수록 alpha를 줄이는 거리다.
    TrailCurveQuality curveQuality{ TrailCurveQuality::Basic }; // trail 곡선 품질 preset 선택값이다.
    float sampleSpacing{ 0.08f };                               // 이동 거리 기준 중간 sample 삽입 간격이다.
    uint32 curveSubdivision{ 4 };                                // 한 frame에서 보강할 최대 중간 sample 수다.
    bool smoothTangent{ true };                                  // 앞뒤 sample 평균으로 곡선을 완만하게 보정할지 결정한다.
    float sideFade{ 0.08f };                                     // 폭 방향 alpha edge 감소 강도다.
};

using AuthoringTypeDataPayload = variant<EmptyTypeData, TrailTypeData>;

struct AuthoringTypeData
{
    AuthoringTypeDataKind kind{ AuthoringTypeDataKind::None };
    AuthoringTypeDataPayload payload{ EmptyTypeData{} };
};

struct RequiredModuleData
{
    string rendererType{ "Sprite" };
    EffectMaterialInstanceData material{};

    Vec3 emitterOrigin{ 0.f, 0.f, 0.f };
    Vec3 emitterRotationDegrees{ 0.f, 0.f, 0.f };

    EmitterScreenAlignment screenAlignment{ EmitterScreenAlignment::Square };
    // AwayFromCenter/Velocity에서 기준 방향을 look으로 쓸지 texture 축으로 쓸지 결정한다.
    EmitterDirectionalAlignmentMode directionalAlignmentMode{ EmitterDirectionalAlignmentMode::TextureAxis };
    // TextureAxis mode에서 기준 방향에 맞출 texture 축이다.
    EmitterSpriteTextureAxis spriteTextureAxis{ EmitterSpriteTextureAxis::Y };
    // texture authored orientation 보정용 정적 roll offset이다.
    float spriteRollOffsetDegrees{ 0.f };

    float minCameraBlendDistance{ 0.f };
    float maxCameraBlendDistance{ 0.f };
    bool useLocalSpace{ false };
    EmitterSortMode sortMode{ EmitterSortMode::None };
    float cameraMotionBlurAmount{ 0.f };
    bool clearExistingParticlesOnInit{ false };
    // Cascade: Emitter Loops. 0 means infinite looping.
    uint32 loopCount{ 0 };
    float duration{ 1.f };
    float durationLow{ 0.f };
    bool useDurationRange{ false };
    bool recalculateDurationEachLoop{ false };
    float delay{ 0.f };
    float delayLow{ 0.f };
    bool useDelayRange{ false };
    bool delayFirstLoopOnly{ false };
    bool killOnDeactivate{ false };
    bool killOnCompleted{ false };
    bool useMaxDrawCount{ true };
    uint32 maxDrawCount{ 500 };
};

struct SpawnModuleData
{
    struct ParticleBurstData
    {
        float time{ 0.f };
        uint32 count{ 16 };
    };

    bool processSpawnRate{ true };
    FloatDistributionData spawnRate{ FloatDistributionData::Make_Constant(10.f) };
    FloatDistributionData spawnRateScale{ FloatDistributionData::Make_Constant(1.f) };
    bool processBurstList{ true };
    vector<ParticleBurstData> burstList{ ParticleBurstData{} };
    FloatDistributionData burstScale{ FloatDistributionData::Make_Constant(1.f) };
    uint32 maxParticleCount{ 64 };
};

struct LifetimeModuleData
{
    FloatDistributionData lifeTime{ FloatDistributionData::Make_Uniform(0.5f, 1.f) };
};

struct InitialLocationModuleData
{
    Vector3DistributionData location{
        Vector3DistributionData::Make_Uniform(Vec3{ -0.5f, -0.5f, -0.5f }, Vec3{ 0.5f, 0.5f, 0.5f })
    };
};

enum class SphereLocationSpawnMode : uint8
{
    Volume,
    Surface,
};

struct SphereLocationModuleData
{
    Vec3 offset{ 0.f, 0.f, 0.f };
    float radius{ 1.f };
    SphereLocationSpawnMode spawnMode{ SphereLocationSpawnMode::Volume };
};

struct InitialSizeModuleData
{
    Vector2DistributionData size{ Vector2DistributionData::Make_Constant(Vec2{ 0.1f, 0.1f }) };
};

struct InitialVelocityModuleData
{
    Vector3DistributionData velocity{ Vector3DistributionData::Make_Constant(Vec3{ 0.f, 0.f, 0.f }) };
    bool inWorldSpace{ false };
};

struct InitialRadialVelocityModuleData
{
    FloatDistributionData speed{ FloatDistributionData::Make_Uniform(0.5f, 1.5f) };
    Vec3 radialPivot{ 0.f, 0.f, 0.f };
    bool inWorldSpace{ false };
};

struct AccelerationModuleData
{
    Vector3DistributionData acceleration{ Vector3DistributionData::Make_Constant(Vec3{ 0.f, 0.f, 0.f }) };
};

struct DragModuleData
{
    FloatDistributionData drag{ FloatDistributionData::Make_Constant(0.f) };
};

struct VelocityOverLifeModuleData
{
    float scaleStart{ 1.f };
    float scaleEnd{ 1.f };
};

struct InitialRotationModuleData
{
    FloatDistributionData rotationDegrees{ FloatDistributionData::Make_Constant(0.f) };
};

struct RotationOverLifeModuleData
{
    float rotationStartDegrees{ 0.f };
    float rotationEndDegrees{ 0.f };
};

struct InitialRotationRateModuleData
{
    FloatDistributionData rotationRateDegrees{ FloatDistributionData::Make_Constant(0.f) };
};

struct RotationRateScaleByLifeModuleData
{
    float scaleStart{ 1.f };
    float scaleEnd{ 1.f };
};

struct InitialColorModuleData
{
    // 색상 RGB와 별도로 생성 시점 alpha를 편집하기 위한 축이다.
    ColorDistributionData color{ ColorDistributionData::Make_Constant(Color{ 1.f, 1.f, 1.f, 1.f }) };
    FloatDistributionData alpha{ FloatDistributionData::Make_Constant(1.f) };
};

struct ColorOverLifeModuleData
{
    // 색상 변화와 독립적으로 fade in/out을 잡기 위한 수명 기준 alpha 축이다.
    ColorDistributionData colorOverLife{ ColorDistributionData::Make_Constant(Color{ 1.f, 1.f, 1.f, 1.f }) };
    FloatDistributionData alphaOverLife{ FloatDistributionData::Make_Constant(1.f) };
};

struct SubUVFrameOverLifeModuleData
{
    FloatDistributionData frameIndex{ FloatDistributionData::Make_ConstantCurve(0.f) };
    uint32 startFrame{ 0 };
    uint32 endFrame{ 0 };
    bool loop{ false };
    SubUVFramePlaybackMode playbackMode{ SubUVFramePlaybackMode::FixedFrame };
    float framesPerSecond{ 12.f };
};

struct SizeByLifeModuleData
{
    float multiplyXStart{ 1.f };
    float multiplyXEnd{ 1.f };
    float multiplyYStart{ 1.f };
    float multiplyYEnd{ 1.f };
};

struct SpawnPerUnitModuleData
{
    bool enabled{ true };
    float unitDistance{ 0.f };
    float maxSampleRate{ 0.f };
};

using AuthoringModuleData = std::variant<
    RequiredModuleData,
    SpawnModuleData,
    LifetimeModuleData,
    InitialLocationModuleData,
    SphereLocationModuleData,
    InitialSizeModuleData,
    InitialVelocityModuleData,
    InitialRadialVelocityModuleData,
    AccelerationModuleData,
    DragModuleData,
    VelocityOverLifeModuleData,
    InitialRotationModuleData,
    RotationOverLifeModuleData,
    InitialRotationRateModuleData,
    RotationRateScaleByLifeModuleData,
    InitialColorModuleData,
    ColorOverLifeModuleData,
    SubUVFrameOverLifeModuleData,
    SizeByLifeModuleData,
    SpawnPerUnitModuleData>;

struct AuthoringModule
{
    uint32 id{};
    AuthoringModuleType type{ AuthoringModuleType::Required };
    string key{};
    wstring displayName{};
    wstring summary{};
    bool enabled{ true };
    bool movable{ true };
    bool removable{ true };
    AuthoringModuleData data{ RequiredModuleData{} };
};

struct AuthoringEmitter
{
    uint32 id{};
    string name{};
    string rendererType{ "Sprite" };
    string resourceSummary{};
    string textureId{ "Effects/Textures/Shared/DefaultTexture.dds" };
    bool enabled{ true };
    bool previewDirty{ true };
    AuthoringTypeData typeData{};
    vector<AuthoringModule> modules{};
};

struct EffectAuthoringSelection
{
    EffectAuthoringSelectionKind kind{ EffectAuthoringSelectionKind::None };
    uint32 emitterId{};
    uint32 moduleId{};
};

struct EffectAuthoringDocument
{
    string name{};
    ParticleSystemAuthoringData particleSystemData{};
    vector<AuthoringEmitter> emitters{};
};

NS_END
