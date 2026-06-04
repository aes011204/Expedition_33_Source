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
    PlaneRadialLocation,
    InitialSize,
    InitialMeshSize,
    InitialVelocity,
    InitialRadialVelocity,
    VelocityCone,
    Acceleration,
    Drag,
    VelocityOverLife,
    OrbitOverLife,
    InitialRotation,
    SphereRadialOrientation,
    PlaneRadialOrientation,
    RotationOverLife,
    InitialRotationRate,
    RotationRateScaleByLife,
    InitialMeshRotation,
    MeshRotationOverLife,
    InitialMeshRotationRate,
    MeshRotationRateScaleByLife,
    InitialColor,
    ColorOverLife,
    SubUVFrameOverLife,
    SizeByLife,
    MeshSizeByLife,
    SpawnPerUnit,
    MaterialScalarModulation,
};

enum class EffectAuthoringSelectionKind : uint8
{
    None,
    Emitter,
    TypeData,
    Module,
};

enum class EmitterRenderLayerOverride : uint8
{
    Auto,
    UIEffect,
};

enum class EmitterScreenAlignment : uint8
{
    Square,
    FacingCameraPosition,
    Rectangle,
    Velocity,
    AwayFromCenter,
    WorldPlaneXY,
    WorldPlaneXZ,
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

enum class EmitterSortPolicy : uint8
{
    None,
    EmitterDepth,
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

enum class DistributionRandomSeedMode : uint8
{
    Default,
    Manual,
};

// 분포/random consumer 모듈이 같은 범위 안에서 재현 가능한 다른 draw를 만들기 위한 seed 설정이다.
struct DistributionRandomSeedData
{
    DistributionRandomSeedMode mode{ DistributionRandomSeedMode::Default };
    uint32 manualSeed{ 0 };
};

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

// 분포 Vec2 상수 커브의 필요 값들이다.
struct Vector2CurveKeyData
{
    float time{ 0.f };      // 0..1 공통축에서 이 key의 위치다.
    Vec2 value{ 0.f, 0.f }; // 이 key 시점의 X/Y 출력값이다.
    Vec2 arriveTangent{};   // key로 들어오는 X/Y 곡선 기울기다.
    Vec2 leaveTangent{};    // key에서 나가는 X/Y 곡선 기울기다.
    FloatCurveInterpolationMode interpolationMode{
        FloatCurveInterpolationMode::Linear
    }; // 이 key 이후 구간의 보간 방식을 결정한다.
};

// 분포 Vec2 상수 커브의 보간 key 목록을 보관한다.
struct ConstantCurveVector2DistributionData
{
    // 분포 Vec2 상수 커브의 authoring key 목록이다.
    vector<Vector2CurveKeyData> keys{
        Vector2CurveKeyData{ 0.f, Vec2{ 0.f, 0.f }, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear },
        Vector2CurveKeyData{ 1.f, Vec2{ 0.f, 0.f }, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear }
    };
};

// 분포 Vec3 상수 커브의 필요 값들이다.
struct Vector3CurveKeyData
{
    float time{ 0.f };           // 0..1 공통축에서 이 key의 위치다.
    Vec3 value{ 0.f, 0.f, 0.f }; // 이 key 시점의 X/Y/Z 출력값이다.
    Vec3 arriveTangent{};        // key로 들어오는 X/Y/Z 곡선 기울기다.
    Vec3 leaveTangent{};         // key에서 나가는 X/Y/Z 곡선 기울기다.
    FloatCurveInterpolationMode interpolationMode{
        FloatCurveInterpolationMode::Linear
    }; // 이 key 이후 구간의 보간 방식을 결정한다.
};

// 분포 Vec3 상수 커브의 보간 key 목록을 보관한다.
struct ConstantCurveVector3DistributionData
{
    // 분포 Vec3 상수 커브의 authoring key 목록이다.
    vector<Vector3CurveKeyData> keys{
        Vector3CurveKeyData{ 0.f, Vec3{ 0.f, 0.f, 0.f }, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear },
        Vector3CurveKeyData{ 1.f, Vec3{ 0.f, 0.f, 0.f }, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear }
    };
};

// Color 분포는 alpha를 별도 Float distribution에서 유지하므로 curve key의 w는 저장/편집하지 않는다.
struct ColorCurveKeyData
{
    float time{ 0.f };                 // 0..1 공통축에서 이 key의 위치다.
    Color value{ 1.f, 1.f, 1.f, 1.f }; // 이 key 시점의 RGB 출력값이다. alpha는 별도 분포가 정본이다.
    Color arriveTangent{};             // key로 들어오는 RGB 곡선 기울기다.
    Color leaveTangent{};              // key에서 나가는 RGB 곡선 기울기다.
    FloatCurveInterpolationMode interpolationMode{
        FloatCurveInterpolationMode::Linear
    }; // 이 key 이후 구간의 보간 방식을 결정한다.
};

// Color RGB 상수 커브의 보간 key 목록을 보관한다.
struct ConstantCurveColorDistributionData
{
    // Color RGB 상수 커브의 authoring key 목록이다.
    vector<ColorCurveKeyData> keys{
        ColorCurveKeyData{ 0.f, Color{ 1.f, 1.f, 1.f, 1.f }, Color{}, Color{}, FloatCurveInterpolationMode::Linear },
        ColorCurveKeyData{ 1.f, Color{ 1.f, 1.f, 1.f, 1.f }, Color{}, Color{}, FloatCurveInterpolationMode::Linear }
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
    DistributionRandomSeedData randomSeed{};
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
        data.payload = UniformFloatDistributionData{ .minValue = minValue, .maxValue = maxValue };
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

    static FloatDistributionData Make_ConstantCurve(float startValue, float endValue)
    {
        FloatDistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveFloatDistributionData{
            {
                FloatCurveKeyData{ 0.f, startValue, 0.f, 0.f, FloatCurveInterpolationMode::Linear },
                FloatCurveKeyData{ 1.f, endValue, 0.f, 0.f, FloatCurveInterpolationMode::Linear }
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
    DistributionRandomSeedData randomSeed{};
};

struct Vector2DistributionData
{
    using Payload = variant<
        ConstantVector2DistributionData,
        UniformVector2DistributionData,
        ConstantCurveVector2DistributionData>;

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
        data.payload = UniformVector2DistributionData{ .minValue = minValue, .maxValue = maxValue };
        return data;
    }

    static Vector2DistributionData Make_ConstantCurve(const Vec2& value)
    {
        Vector2DistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveVector2DistributionData{
            {
                Vector2CurveKeyData{ 0.f, value, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear },
                Vector2CurveKeyData{ 1.f, value, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }

    static Vector2DistributionData Make_ConstantCurve(const Vec2& startValue, const Vec2& endValue)
    {
        Vector2DistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveVector2DistributionData{
            {
                Vector2CurveKeyData{ 0.f, startValue, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear },
                Vector2CurveKeyData{ 1.f, endValue, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear }
            }
        };
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
    DistributionRandomSeedData randomSeed{};
};

struct Vector3DistributionData
{
    using Payload = variant<
        ConstantVector3DistributionData,
        UniformVector3DistributionData,
        ConstantCurveVector3DistributionData>;

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
        data.payload = UniformVector3DistributionData{ .minValue = minValue, .maxValue = maxValue };
        return data;
    }

    static Vector3DistributionData Make_ConstantCurve(const Vec3& value)
    {
        Vector3DistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveVector3DistributionData{
            {
                Vector3CurveKeyData{ 0.f, value, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear },
                Vector3CurveKeyData{ 1.f, value, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }

    static Vector3DistributionData Make_ConstantCurve(const Vec3& startValue, const Vec3& endValue)
    {
        Vector3DistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveVector3DistributionData{
            {
                Vector3CurveKeyData{ 0.f, startValue, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear },
                Vector3CurveKeyData{ 1.f, endValue, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear }
            }
        };
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
    DistributionRandomSeedData randomSeed{};
};

struct ColorDistributionData
{
    using Payload = variant<
        ConstantColorDistributionData,
        UniformColorDistributionData,
        ConstantCurveColorDistributionData>;

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
        data.payload = UniformColorDistributionData{ .minValue = minValue, .maxValue = maxValue };
        return data;
    }

    static ColorDistributionData Make_ConstantCurve(const Color& value)
    {
        ColorDistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveColorDistributionData{
            {
                ColorCurveKeyData{ 0.f, value, Color{}, Color{}, FloatCurveInterpolationMode::Linear },
                ColorCurveKeyData{ 1.f, value, Color{}, Color{}, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }

    static ColorDistributionData Make_ConstantCurve(const Color& startValue, const Color& endValue)
    {
        ColorDistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveColorDistributionData{
            {
                ColorCurveKeyData{ 0.f, startValue, Color{}, Color{}, FloatCurveInterpolationMode::Linear },
                ColorCurveKeyData{ 1.f, endValue, Color{}, Color{}, FloatCurveInterpolationMode::Linear }
            }
        };
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
    Mesh,
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
    float uvTiling{ 1.f };                                      // body/mask가 아니라 noise/breakup 길이 방향 반복 밀도다.
    float maxTrailLength{ 0.f };                                // 0 이하면 head 기준 최대 표시 길이를 제한하지 않는다.
    float tailFadeLength{ 0.1f };                               // maxTrailLength 끝으로 갈수록 alpha를 줄이는 거리다.
    TrailCurveQuality curveQuality{ TrailCurveQuality::Basic }; // trail 곡선 품질 preset 선택값이다.
    float sampleSpacing{ 0.08f };                               // 이동 거리 기준 중간 sample 삽입 간격이다.
    uint32 curveSubdivision{ 4 };                               // 한 frame에서 보강할 최대 중간 sample 수다.
    bool smoothTangent{ true };                                 // 앞뒤 sample 평균으로 곡선을 완만하게 보정할지 결정한다.
    float sideFade{ 0.08f };                                    // 폭 방향 alpha edge 감소 강도다.
};

struct MeshTypeData
{
    string modelGuid{};
    string modelPath{};
    string assignedEffectMaterialGuid{};
    string assignedEffectMaterialPath{};
    bool hasAssignedMaterialInstance{ false };
    EffectMaterialInstanceData assignedMaterial{};
    Vec3 previewScale{ 1.f, 1.f, 1.f };
    bool useModelMaterials{ false };
};

using AuthoringTypeDataPayload = variant<EmptyTypeData, TrailTypeData, MeshTypeData>;

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

    EmitterScreenAlignment screenAlignment{ EmitterScreenAlignment::FacingCameraPosition };
    // AwayFromCenter/Velocity에서 기준 방향을 look으로 쓸지 texture 축으로 쓸지 결정한다.
    EmitterDirectionalAlignmentMode directionalAlignmentMode{ EmitterDirectionalAlignmentMode::TextureAxis };
    // TextureAxis mode에서 기준 방향에 맞출 texture 축이다.
    EmitterSpriteTextureAxis spriteTextureAxis{ EmitterSpriteTextureAxis::Y };
    // texture authored orientation 보정용 정적 roll offset이다.
    float spriteRollOffsetDegrees{ 0.f };

    float minCameraBlendDistance{ 0.f };
    float maxCameraBlendDistance{ 0.f };
    bool useLocalSpace{ true };
    EmitterSortPolicy sortPolicy{ EmitterSortPolicy::None };
    int32 sortLayer{ 0 };
    EmitterSortMode sortMode{ EmitterSortMode::None };
    float sortBias{ 0.f };
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
    Vector3DistributionData location{ Vector3DistributionData::Make_Constant(Vec3{ 0.f, 0.f, 0.f }) };
};

enum class SphereLocationSpawnMode : uint8
{
    Volume,
    Surface,
};

enum class SphereLocationPlacementMode : uint8
{
    Random,
    EvenByParticleIndex,
};

struct SphereLocationModuleData
{
    Vec3 offset{ 0.f, 0.f, 0.f };
    float radius{ 1.f };
    SphereLocationSpawnMode spawnMode{ SphereLocationSpawnMode::Volume };
    SphereLocationPlacementMode placementMode{ SphereLocationPlacementMode::Random };
    DistributionRandomSeedData randomSeed{};
};

enum class PlaneRadialLocationPlane : uint8
{
    XY,
    XZ,
    YZ,
};

enum class PlaneRadialLocationShape : uint8
{
    Rectangle,
    Disc,
    Ring,
    Arc,
};

enum class PlaneRadialLocationPlacementMode : uint8
{
    Random,
    EvenByParticleIndex,
};

struct PlaneRadialLocationModuleData
{
    bool enabled{ true };
    PlaneRadialLocationPlane plane{ PlaneRadialLocationPlane::XY };
    PlaneRadialLocationShape shape{ PlaneRadialLocationShape::Disc };
    PlaneRadialLocationPlacementMode placementMode{ PlaneRadialLocationPlacementMode::Random };
    Vec3 offset{ 0.f, 0.f, 0.f };
    FloatDistributionData uDistribution{ FloatDistributionData::Make_Uniform(-0.5f, 0.5f) };
    FloatDistributionData vDistribution{ FloatDistributionData::Make_Uniform(-0.5f, 0.5f) };
    FloatDistributionData radiusDistribution{ FloatDistributionData::Make_Uniform(0.f, 1.f) };
    FloatDistributionData angleDegreesDistribution{ FloatDistributionData::Make_Uniform(0.f, 360.f) };
    float thickness{ 0.f };
};

enum class PlaneRadialOrientationTargetKind : uint8
{
    Auto,
    Sprite2D,
    Mesh3D,
};

enum class PlaneRadialOrientationMode : uint8
{
    None,
    FaceRadialOut,
    FaceRadialIn,
    FaceTangentCW,
    FaceTangentCCW,
    FacePlaneNormal,
};

enum class PlaneRadialOrientationAxis : uint8
{
    PositiveX,
    NegativeX,
    PositiveY,
    NegativeY,
    PositiveZ,
    NegativeZ,
};

struct PlaneRadialOrientationModuleData
{
    bool enabled{ true };
    PlaneRadialOrientationTargetKind targetKind{ PlaneRadialOrientationTargetKind::Auto };
    PlaneRadialOrientationMode orientationMode{ PlaneRadialOrientationMode::None };
    PlaneRadialOrientationAxis meshForwardAxis{ PlaneRadialOrientationAxis::PositiveZ };
    PlaneRadialOrientationAxis meshUpAxis{ PlaneRadialOrientationAxis::PositiveY };
    float tiltDegrees{ 0.f };
    float rollOffsetDegrees{ 0.f };
};

enum class SphereRadialOrientationMode : uint8
{
    None,
    FaceRadialOut,
    FaceRadialIn,
};

struct SphereRadialOrientationModuleData
{
    bool enabled{ true };
    SphereRadialOrientationMode orientationMode{ SphereRadialOrientationMode::None };
    PlaneRadialOrientationAxis meshForwardAxis{ PlaneRadialOrientationAxis::PositiveZ };
    PlaneRadialOrientationAxis meshUpAxis{ PlaneRadialOrientationAxis::PositiveY };
    float tiltDegrees{ 0.f };
    float rollOffsetDegrees{ 0.f };
};

struct InitialSizeModuleData
{
    Vector2DistributionData size{ Vector2DistributionData::Make_Constant(Vec2{ 0.3f, 0.3f }) };
};

struct InitialMeshSizeModuleData
{
    Vector3DistributionData size{ Vector3DistributionData::Make_Constant(Vec3{ 0.1f, 0.1f, 0.1f }) };
};

struct InitialVelocityModuleData
{
    Vector3DistributionData velocity{ Vector3DistributionData::Make_Constant(Vec3{ 0.f, 0.f, 0.f }) };
    bool inWorldSpace{ false };
};

struct InitialRadialVelocityModuleData
{
    FloatDistributionData speed{ FloatDistributionData::Make_Uniform(1.f, 2.f) };
    Vec3 radialPivot{ 0.f, 0.f, 0.f };
    bool inWorldSpace{ false };
};

struct VelocityConeModuleData
{
    Vec3 axis{ 0.f, 1.f, 0.f };
    float angleDegrees{ 30.f };
    FloatDistributionData speed{ FloatDistributionData::Make_Uniform(1.f, 2.f) };
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
    FloatDistributionData scaleOverLife{ FloatDistributionData::Make_ConstantCurve(1.f, 1.f) };
};

enum class EffectOrbitPivotMode : uint8
{
    EmitterOrigin,
    SpawnOrigin,
};

enum class EffectOrbitPlane : uint8
{
    XY,
    XZ,
    YZ,
};

enum class EffectOrbitOrientationMode : uint8
{
    Preserve,
    InheritOrbitAngle,
    FaceRadialOut,
    FaceTangent,
};

struct OrbitOverLifeModuleData
{
    bool enabled{ true };
    EffectOrbitPivotMode pivotMode{ EffectOrbitPivotMode::EmitterOrigin };
    EffectOrbitPlane plane{ EffectOrbitPlane::XY };
    FloatDistributionData angleDegreesOverLife{ FloatDistributionData::Make_ConstantCurve(0.f, 0.f) };
    FloatDistributionData radiusScaleOverLife{ FloatDistributionData::Make_ConstantCurve(1.f, 1.f) };
    EffectOrbitOrientationMode orientationMode{ EffectOrbitOrientationMode::Preserve };
};

struct InitialRotationModuleData
{
    FloatDistributionData rotationDegrees{ FloatDistributionData::Make_Constant(0.f) };
};

struct RotationOverLifeModuleData
{
    FloatDistributionData rotationOverLife{ FloatDistributionData::Make_ConstantCurve(0.f, 0.f) };
};

struct InitialRotationRateModuleData
{
    FloatDistributionData rotationRateDegrees{ FloatDistributionData::Make_Constant(0.f) };
};

struct RotationRateScaleByLifeModuleData
{
    FloatDistributionData scaleOverLife{ FloatDistributionData::Make_ConstantCurve(1.f, 1.f) };
};

struct InitialMeshRotationModuleData
{
    Vector3DistributionData rotationDegrees{ Vector3DistributionData::Make_Constant(Vec3{ 0.f, 0.f, 0.f }) };
};

struct MeshRotationOverLifeModuleData
{
    Vector3DistributionData rotationOverLife{ Vector3DistributionData::Make_ConstantCurve(Vec3{ 0.f, 0.f, 0.f }) };
};

struct InitialMeshRotationRateModuleData
{
    Vector3DistributionData rotationRateDegrees{ Vector3DistributionData::Make_Constant(Vec3{ 0.f, 0.f, 0.f }) };
};

struct MeshRotationRateScaleByLifeModuleData
{
    Vector3DistributionData scaleOverLife{ Vector3DistributionData::Make_ConstantCurve(Vec3{ 1.f, 1.f, 1.f }) };
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
    DistributionRandomSeedData randomSeed{};
};

struct SizeByLifeModuleData
{
    enum class AxisLock : uint8
    {
        None,
        X,
        Y,
    };

    Vector2DistributionData scaleOverLife{ Vector2DistributionData::Make_ConstantCurve(Vec2{ 1.f, 1.f }) };
    bool multiplyX{ true };
    bool multiplyY{ true };
    AxisLock axisLock{ AxisLock::None };
};

struct MeshSizeByLifeModuleData
{
    Vector3DistributionData scaleOverLife{ Vector3DistributionData::Make_ConstantCurve(Vec3{ 1.f, 1.f, 1.f }) };
    bool multiplyX{ true };
    bool multiplyY{ true };
    bool multiplyZ{ true };
};

struct SpawnPerUnitModuleData
{
    bool enabled{ true };
    FloatDistributionData spawnPerUnit{ FloatDistributionData::Make_Constant(50.f) };
    float unitScalar{ 1.f };
    float movementTolerance{ 0.001f };
    float maxFrameDistance{ 0.f };
};

enum class MaterialScalarModulationTargetField : uint8
{
    Intensity,
    OpacityPower,
    NoiseStrength,
    AlphaErosion,
    AlphaCutoff,
    CoreIntensity,
    OuterIntensity,
    CoreColor,
};

enum class MaterialScalarModulationOperation : uint8
{
    Multiply,
};

enum class MaterialScalarModulationTimeSource : uint8
{
    ParticleLife,
    EmitterTime,
};

struct MaterialScalarModulatorData
{
    bool enabled{ true };
    MaterialScalarModulationTargetField targetField{ MaterialScalarModulationTargetField::Intensity };
    MaterialScalarModulationOperation operation{ MaterialScalarModulationOperation::Multiply };
    MaterialScalarModulationTimeSource timeSource{ MaterialScalarModulationTimeSource::EmitterTime };
    FloatDistributionData distribution{ FloatDistributionData::Make_Constant(1.f) };
};

struct MaterialCoreColorRgbModulatorData
{
    bool enabled{ true };
    MaterialScalarModulationTimeSource timeSource{ MaterialScalarModulationTimeSource::EmitterTime };
    Vector3DistributionData distribution{ Vector3DistributionData::Make_Constant(Vec3{ 1.f, 0.85f, 0.45f }) };
};

struct MaterialScalarModulationModuleData
{
    vector<MaterialScalarModulatorData> modulators{};
    vector<MaterialCoreColorRgbModulatorData> coreColorRgbModulators{};
};

using AuthoringModuleData = std::variant<
    RequiredModuleData,
    SpawnModuleData,
    LifetimeModuleData,
    InitialLocationModuleData,
    SphereLocationModuleData,
    PlaneRadialLocationModuleData,
    InitialSizeModuleData,
    InitialMeshSizeModuleData,
    InitialVelocityModuleData,
    InitialRadialVelocityModuleData,
    VelocityConeModuleData,
    AccelerationModuleData,
    DragModuleData,
    VelocityOverLifeModuleData,
    OrbitOverLifeModuleData,
    InitialRotationModuleData,
    SphereRadialOrientationModuleData,
    PlaneRadialOrientationModuleData,
    RotationOverLifeModuleData,
    InitialRotationRateModuleData,
    RotationRateScaleByLifeModuleData,
    InitialMeshRotationModuleData,
    MeshRotationOverLifeModuleData,
    InitialMeshRotationRateModuleData,
    MeshRotationRateScaleByLifeModuleData,
    InitialColorModuleData,
    ColorOverLifeModuleData,
    SubUVFrameOverLifeModuleData,
    SizeByLifeModuleData,
    MeshSizeByLifeModuleData,
    SpawnPerUnitModuleData,
    MaterialScalarModulationModuleData>;

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
    EmitterRenderLayerOverride renderLayerOverride{ EmitterRenderLayerOverride::Auto };
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
