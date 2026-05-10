#pragma once

#include <variant>

NS_BEGIN(EffectEditor)

enum class AuthoringModuleType : uint8
{
    Required,
    Spawn,
    Lifetime,
    InitialSize,
    InitialVelocity,
    ColorOverLife,
};

enum class EffectAuthoringSelectionKind : uint8
{
    None,
    Emitter,
    Module,
};

enum class EmitterScreenAlignment : uint8
{
    Square,
    CameraFacing,
    Velocity,
};

enum class EmitterSortMode : uint8
{
    None,
    ViewDepth,
    Age,
};

enum class DistributionValueKind : uint8
{
    Float,
    Vector2,
    Vector3,
};

enum class DistributionMode : uint8
{
    Constant,
    Uniform,
    ConstantCurveReserved,
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
    using Payload = variant<ConstantFloatDistributionData, UniformFloatDistributionData>;

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

struct ParticleSystemAuthoringData
{
    bool autoDeactivate{ true };
};

struct RequiredModuleData
{
    string rendererType{ "Sprite" };
    string materialName{ "DefaultSpriteMaterial" };
    string textureId{ "Texture_Snow" };
    Vec3 emitterOrigin{ 0.f, 0.f, 0.f };
    Vec3 emitterRotationDegrees{ 0.f, 0.f, 0.f };
    EmitterScreenAlignment screenAlignment{ EmitterScreenAlignment::Square };
    float minCameraBlendDistance{ 0.f };
    float maxCameraBlendDistance{ 0.f };
    bool useLocalSpace{ false };
    EmitterSortMode sortMode{ EmitterSortMode::None };
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
    FloatDistributionData lifeTime{ FloatDistributionData::Make_Uniform(1.f, 2.f) };
};

struct InitialSizeModuleData
{
    Vector2DistributionData size{ Vector2DistributionData::Make_Constant(Vec2{ 0.15f, 0.35f }) };
};

struct InitialVelocityModuleData
{
    Vec3 radialPivot{ 0.f, 2.f, 0.f };
    FloatDistributionData speed{ FloatDistributionData::Make_Uniform(0.5f, 1.5f) };
};

struct ColorOverLifeModuleData
{
    Vector3DistributionData colorOverLife{ Vector3DistributionData::Make_Constant(Vec3{ 1.f, 1.f, 1.f }) };
    FloatDistributionData alphaOverLife{ FloatDistributionData::Make_Constant(1.f) };
};

using AuthoringModuleData = std::variant<
    RequiredModuleData,
    SpawnModuleData,
    LifetimeModuleData,
    InitialSizeModuleData,
    InitialVelocityModuleData,
    ColorOverLifeModuleData>;

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
    string materialName{ "DefaultSpriteMaterial" };
    string textureId{ "Texture_Snow" };
    bool enabled{ true };
    bool previewDirty{ true };
    vector<AuthoringModule> modules{};
};

struct EffectAuthoringSelection
{
    EffectAuthoringSelectionKind kind{ EffectAuthoringSelectionKind::None };
    uint32 emitterId{};
    uint32 moduleId{};
};

NS_END
