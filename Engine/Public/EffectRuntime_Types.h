#pragma once
#include <array>
#include <variant>
#include "EffectMaterial_Types.h"
#include "Engine_Typedef.h"
#include "GameObject.h"
#include "Particle_Types.h"

NS_BEGIN(Engine)

//## Emitter::Kind

enum class EffectEmitterKind : uint8
{
    Sprite,
    Trail,
    Ribbon,
    SourceHistorySpriteTrail,
    Beam,
    Mesh,
};

enum class EffectEmitterRenderLayerOverride : uint8
{
    Auto,
    UIEffect,
};

enum class EffectHistoryBudgetPreset : uint8
{
    Full,
    High,
    Medium,
    Performance,
    Low,
};

enum class EffectHistoryBudgetPriority : uint8
{
    Core,
    Secondary,
    Decorative,
    Distortion,
};

enum class EffectHistoryBudgetDensityBias : uint8
{
    Dense,
    Normal,
    Sparse,
};

enum class EffectHistoryBudgetUpdateRate : uint8
{
    EveryFrame,
    Every2Frames,
};

enum class EffectHistoryBudgetFamily : uint8
{
    Trail,
    Ribbon,
    SourceHistorySpriteTrail,
    Distortion,
};

struct EffectHistoryBudgetRuntimeDesc
{
    EffectHistoryBudgetPreset preset{ EffectHistoryBudgetPreset::Full };
    bool preserveLength{ true };
    bool sharedSourceHistory{ true };
    float trailDensityScale{ 1.f };
    float spriteStampDensityScale{ 1.f };
    float ribbonDensityScale{ 1.f };
    float distortionDensityScale{ 1.f };
    EffectHistoryBudgetUpdateRate updateRate{ EffectHistoryBudgetUpdateRate::EveryFrame };
};

struct EffectEmitterHistoryBudgetRuntimeDesc
{
    EffectHistoryBudgetPriority priority{ EffectHistoryBudgetPriority::Core };
    EffectHistoryBudgetDensityBias densityBias{ EffectHistoryBudgetDensityBias::Normal };
    float lengthScale{ 1.f };
};

struct EffectHistoryBudgetEffectiveInput
{
    EffectHistoryBudgetFamily family{ EffectHistoryBudgetFamily::Trail };
    uint32 sourceSampleCount{};
    uint32 renderSegmentCount{};
    uint32 spriteStampCount{};
};

struct EffectHistoryBudgetEffectiveDesc
{
    EffectHistoryBudgetFamily family{ EffectHistoryBudgetFamily::Trail };
    uint32 sourceSampleCount{};
    uint32 renderSegmentCount{};
    uint32 spriteStampCount{};
};

enum class EffectHistorySourceGroupKind : uint8
{
    None,
    TrailPairHistory,
    SourcePointHistory,
};

struct EffectHistorySourceGroupKey
{
    EffectHistorySourceGroupKind kind{ EffectHistorySourceGroupKind::None };
    string stableId{};
};

struct EffectHistorySourceGroupStats
{
    EffectHistorySourceGroupKind kind{ EffectHistorySourceGroupKind::None };
    uint32 sourceRequestCount{};
    uint32 cacheHitCount{};
    uint32 savedRequestCount{};
};

struct EffectHistorySourceGroupBudgetDesc
{
    EffectHistorySourceGroupKind kind{ EffectHistorySourceGroupKind::None };
    string stableId{};
    bool confirmed{};

    uint32 sourceSampleCount{};
    float sampleLifetime{};
    float sampleSpacing{};
    float sampleInterval{};
    uint32 curveSubdivision{};
    bool smoothTangent{ true };
};

inline const EffectHistorySourceGroupBudgetDesc* Find_EffectHistorySourceGroupBudget(
    const vector<EffectHistorySourceGroupBudgetDesc>& budgets,
    EffectHistorySourceGroupKind kind,
    const string& stableId)
{
    for (const EffectHistorySourceGroupBudgetDesc& budget : budgets)
    {
        if (budget.kind == kind && budget.stableId == stableId)
            return &budget;
    }

    return nullptr;
}

inline float Resolve_EffectHistoryBudgetPresetScale(EffectHistoryBudgetPreset preset)
{
    switch (preset)
    {
    case EffectHistoryBudgetPreset::High:
        return 0.85f;
    case EffectHistoryBudgetPreset::Medium:
        return 0.7f;
    case EffectHistoryBudgetPreset::Performance:
        return 0.5f;
    case EffectHistoryBudgetPreset::Low:
        return 0.35f;
    case EffectHistoryBudgetPreset::Full:
    default:
        return 1.f;
    }
}

inline float Resolve_EffectHistoryBudgetFamilyScale(
    const EffectHistoryBudgetRuntimeDesc& budget,
    EffectHistoryBudgetFamily family)
{
    switch (family)
    {
    case EffectHistoryBudgetFamily::Ribbon:
        return budget.ribbonDensityScale;
    case EffectHistoryBudgetFamily::Distortion:
        return budget.distortionDensityScale;
    case EffectHistoryBudgetFamily::SourceHistorySpriteTrail:
    case EffectHistoryBudgetFamily::Trail:
    default:
        return budget.trailDensityScale;
    }
}

inline float Resolve_EffectHistoryBudgetBiasScale(EffectHistoryBudgetDensityBias bias)
{
    switch (bias)
    {
    case EffectHistoryBudgetDensityBias::Dense:
        return 1.25f;
    case EffectHistoryBudgetDensityBias::Sparse:
        return 0.75f;
    case EffectHistoryBudgetDensityBias::Normal:
    default:
        return 1.f;
    }
}

inline uint32 Scale_EffectHistoryBudgetCount(uint32 value, float scale)
{
    if (value == 0u)
        return 0u;

    const float scaled = static_cast<float>(value) * scale;
    return static_cast<uint32>(scaled + 0.999f) < 1u ? 1u : static_cast<uint32>(scaled + 0.999f);
}

inline EffectHistoryBudgetEffectiveDesc Compute_EffectHistoryBudgetEffective(
    const EffectHistoryBudgetRuntimeDesc& budget,
    const EffectEmitterHistoryBudgetRuntimeDesc& usage,
    const EffectHistoryBudgetEffectiveInput& input)
{
    EffectHistoryBudgetEffectiveDesc effective{};
    effective.family = input.family;

    if (budget.preset == EffectHistoryBudgetPreset::Full)
    {
        effective.sourceSampleCount = input.sourceSampleCount;
        effective.renderSegmentCount = input.renderSegmentCount;
        effective.spriteStampCount = input.spriteStampCount;
        return effective;
    }

    const float scale =
        Resolve_EffectHistoryBudgetPresetScale(budget.preset) *
        Resolve_EffectHistoryBudgetFamilyScale(budget, input.family) *
        Resolve_EffectHistoryBudgetBiasScale(usage.densityBias);

    effective.sourceSampleCount = Scale_EffectHistoryBudgetCount(input.sourceSampleCount, scale);
    effective.renderSegmentCount = Scale_EffectHistoryBudgetCount(input.renderSegmentCount, scale);
    effective.spriteStampCount = input.spriteStampCount;
    return effective;
}

enum class EffectSortPolicy : uint8
{
    None,
    EmitterDepth,
};

//## Trail::SampleAndProvider

struct EffectTrailSample
{
    Vec3 baseWorldPosition{};
    Vec3 tipWorldPosition{};
    Quat baseWorldRotation{ Quat::Identity };
    Quat tipWorldRotation{ Quat::Identity };
};

struct EffectSourcePointSample
{
    Vec3 worldPosition{};
    Quat worldRotation{ Quat::Identity };
};

class ENGINE_DLL IEffectTrailSampleProvider
{
public:
    virtual ~IEffectTrailSampleProvider() = default;
    virtual bool Try_GetTrailSample(EffectTrailSample& outSample) const = 0;
};

class ENGINE_DLL IEffectSourcePointSampleProvider
{
public:
    virtual ~IEffectSourcePointSampleProvider() = default;
    virtual bool Try_GetSourcePointSample(EffectSourcePointSample& outSample) const = 0;
};

//## Module::SpriteAlignment

enum class EffectScreenAlignment : uint32
{
    FacingCameraPosition = 0,
    Rectangle = 1,
    Square = 2,
    AwayFromCenter = 3,
    Velocity = 4,
    WorldPlaneXY = 5,
    WorldPlaneXZ = 6,
    WorldUpFacingCamera = 7,
};

enum class EffectSpriteDirectionalAlignmentMode : uint32
{
    LookDirection = 0,
    TextureAxis = 1,
};

enum class EffectSpriteTextureAxis : uint32
{
    X = 0,
    Y = 1,
};

//## Module::MaterialModulation

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
    RefractionIntensity = 8,
    MainUVScrollSpeedScale = 9,
    NoiseUVScrollSpeedScale = 10,
    MaskUVScrollSpeedScale = 11,
    FlowUVScrollSpeedScale = 12,
    AlphaMultiplier = 13,
};

enum class EffectMaterialVec2ModulationTarget : uint32
{
    MainUVOffset = 0,
    NoiseUVOffset = 1,
    MaskUVOffset = 2,
    FlowUVOffset = 3,
};

enum class EffectMaterialVec2ModulationOperation : uint32
{
    Add = 0,
};

enum class EffectMaterialScalarModulationOperation : uint32
{
    Multiply = 0,
};

enum class EffectMaterialScalarModulationTimeSource : uint32
{
    ParticleLife = 0,
    EmitterTime = 1,
};

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
    Vec4 keyModes{ 1.f, 1.f, 0.f, 0.f };
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

struct EffectMaterialScalarModulationRuntimeDesc
{
    static constexpr uint32 kMaxModulators{ 8 };

    uint32 count{ 0 };
    std::array<EffectMaterialScalarModulatorRuntimeDesc, kMaxModulators> modulators{};
};

struct EffectMaterialCoreColorRgbModulationCurveDesc
{
    bool enabled{ false };
    bool uniformEnabled{ false };
    Vec3 uniformMin{ 1.f, 0.85f, 0.45f };
    Vec3 uniformMax{ 1.f, 0.85f, 0.45f };
    PointParticleRandomSeedRuntimeDesc uniformSeed{};
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
    Vec4 keyModes{ 1.f, 1.f, 0.f, 0.f };
    Vec4 keyModesBlock1{};
};

struct EffectMaterialCoreColorRgbModulatorRuntimeDesc
{
    bool enabled{ true };
    EffectMaterialScalarModulationTimeSource timeSource{ EffectMaterialScalarModulationTimeSource::EmitterTime };
    EffectMaterialCoreColorRgbModulationCurveDesc curve{};
};

struct EffectMaterialCoreColorRgbModulationRuntimeDesc
{
    static constexpr uint32 kMaxModulators{ 8 };

    uint32 count{ 0 };
    std::array<EffectMaterialCoreColorRgbModulatorRuntimeDesc, kMaxModulators> modulators{};
};

struct EffectMaterialVec2ModulationCurveDesc
{
    bool enabled{ false };
    uint32 keyCount{ 2 };
    Vec4 keyTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 keyTimesBlock1{};
    Vec4 keyValuesX{ 0.f, 0.f, 0.f, 0.f };
    Vec4 keyValuesXBlock1{};
    Vec4 keyValuesY{ 0.f, 0.f, 0.f, 0.f };
    Vec4 keyValuesYBlock1{};
    Vec4 keyArriveTangentsX{};
    Vec4 keyArriveTangentsXBlock1{};
    Vec4 keyArriveTangentsY{};
    Vec4 keyArriveTangentsYBlock1{};
    Vec4 keyLeaveTangentsX{};
    Vec4 keyLeaveTangentsXBlock1{};
    Vec4 keyLeaveTangentsY{};
    Vec4 keyLeaveTangentsYBlock1{};
    Vec4 keyModes{ 1.f, 1.f, 0.f, 0.f };
    Vec4 keyModesBlock1{};
};

struct EffectMaterialVec2ModulatorRuntimeDesc
{
    bool enabled{ true };
    EffectMaterialVec2ModulationTarget targetField{ EffectMaterialVec2ModulationTarget::MainUVOffset };
    EffectMaterialVec2ModulationOperation operation{ EffectMaterialVec2ModulationOperation::Add };
    EffectMaterialScalarModulationTimeSource timeSource{ EffectMaterialScalarModulationTimeSource::EmitterTime };
    EffectMaterialVec2ModulationCurveDesc curve{};
};

struct EffectMaterialVec2ModulationRuntimeDesc
{
    static constexpr uint32 kMaxModulators{ 8 };

    uint32 count{ 0 };
    std::array<EffectMaterialVec2ModulatorRuntimeDesc, kMaxModulators> modulators{};
};

//## Module::Required

struct EffectRequiredMaterialRuntimeDesc
{
    EffectMaterialFamily materialFamily{
        EffectMaterialFamily::Default };
    string mainTextureGuid{};
    string mainTexturePath{};
    string noiseTextureGuid{};
    string noiseTexturePath{};
    string maskTextureGuid{};
    string maskTexturePath{};
    string flowTextureGuid{};
    string flowTexturePath{};

    EffectMaterialBlendMode blendMode{
        EffectMaterialBlendMode::AlphaBlend };
    string opacitySource{ "Alpha" };
    Vec4 tint{ 1.f, 1.f, 1.f, 1.f };
    float intensity{ 1.f };
    float opacityPower{ 1.f };
    float alphaMultiplier{ 1.f };
    float noiseStrength{ 0.f };
    float alphaCutoff{ 0.f };
    float alphaErosion{ 0.f };
    string noiseSource{ "Red" };
    string maskSource{ "Alpha" };
    bool noiseInvert{ false };
    bool maskInvert{ false };

    Vec2 mainUVScale{ 1.f, 1.f };
    Vec2 mainUVOffset{ 0.f, 0.f };
    Vec2 mainUVScrollSpeed{ 0.f, 0.f };
    EffectTextureUVTilingMode mainUVTilingMode{
        EffectTextureUVTilingMode::Wrap };
    EffectMaterialUVAxisPolicy mainUVPolicy{
        EffectTextureUVTilingMode::Wrap,
        EffectTextureUVTilingMode::Wrap };
    EffectMaterialUVRotation mainUVRotation{
        EffectMaterialUVRotation::None };

    Vec2 noiseUVScale{ 1.f, 1.f };
    Vec2 noiseUVOffset{ 0.f, 0.f };
    Vec2 noiseUVScrollSpeed{ 0.f, 0.f };
    EffectTextureUVTilingMode noiseUVTilingMode{
        EffectTextureUVTilingMode::Wrap };
    EffectMaterialUVAxisPolicy noiseUVPolicy{
        EffectTextureUVTilingMode::Wrap,
        EffectTextureUVTilingMode::Wrap };
    EffectMaterialUVRotation noiseUVRotation{
        EffectMaterialUVRotation::None };

    Vec2 maskUVScale{ 1.f, 1.f };
    Vec2 maskUVOffset{ 0.f, 0.f };
    Vec2 maskUVScrollSpeed{ 0.f, 0.f };
    EffectTextureUVTilingMode maskUVTilingMode{
        EffectTextureUVTilingMode::Wrap };
    EffectMaterialUVAxisPolicy maskUVPolicy{
        EffectTextureUVTilingMode::Wrap,
        EffectTextureUVTilingMode::Wrap };
    EffectMaterialUVRotation maskUVRotation{
        EffectMaterialUVRotation::None };

    Vec2 flowUVScale{ 1.f, 1.f };
    Vec2 flowUVOffset{ 0.f, 0.f };
    Vec2 flowUVScrollSpeed{ 0.f, 0.f };
    EffectTextureUVTilingMode flowUVTilingMode{
        EffectTextureUVTilingMode::Wrap };
    EffectMaterialUVAxisPolicy flowUVPolicy{
        EffectTextureUVTilingMode::Wrap,
        EffectTextureUVTilingMode::Wrap };
    EffectMaterialUVRotation flowUVRotation{
        EffectMaterialUVRotation::None };

    EffectMaterialAdditiveContributionData additive{};
    EffectMaterialCoreEmissiveData coreEmissive{};
    float refractionIntensity{ 0.f };
    float refractionPresence{ 1.f };
    EffectDistortionShapeMode distortionShapeMode{
        EffectDistortionShapeMode::None };
    EffectAirSheathMapInterpretation airSheathMapInterpretation{
        EffectAirSheathMapInterpretation::VectorField };
    EffectAirSheathMapScalarSource airSheathMapXSource{
        EffectAirSheathMapScalarSource::Red };
    EffectAirSheathMapScalarSource airSheathMapYSource{
        EffectAirSheathMapScalarSource::Green };
    EffectAirSheathMapVectorSpace airSheathMapVectorSpace{
        EffectAirSheathMapVectorSpace::Screen };
    EffectAirSheathMapComposition airSheathMapComposition{
        EffectAirSheathMapComposition::Replace };
    float airSheathMapInfluence{ 0.f };
    float distortionShapeRadius{ 0.45f };
    float distortionShapeThickness{ 0.10f };
    float distortionShapeSoftness{ 0.15f };
    float glassAlpha{ 0.18f };
    float glassAlphaPower{ 1.f };
    float glassNormalStrength{ 1.f };
    Color glassRimColor{ 1.f, 1.f, 1.f, 1.f };
    float glassRimIntensity{ 1.5f };
    float glassRimPower{ 3.f };
    Vec3 glassLightDirection{ -0.35f, 0.65f, -0.65f };
    Color glassLightColor{ 1.f, 1.f, 1.f, 1.f };
    float glassLightIntensity{ 1.2f };
    float glassSpecularPower{ 48.f };
    float glassSpecularSoftness{ 1.f };
    float glassMainInfluence{ 0.25f };
    float glassNoiseBreakup{ 0.35f };
    float glassMaskStrength{ 1.f };
    bool twoSided{ false };
    uint32 subUVRows{ 1 };
    uint32 subUVCols{ 1 };
    EffectMaterialCoreColorRgbModulationRuntimeDesc coreColorRgbModulation{};
    EffectMaterialVec2ModulationRuntimeDesc vec2Modulation{};
    EffectMaterialScalarModulationRuntimeDesc scalarModulation{};
};

struct EffectSpriteRenderRuntimeDesc
{
    EffectScreenAlignment screenAlignment{ EffectScreenAlignment::FacingCameraPosition };
    EffectSpriteDirectionalAlignmentMode directionalAlignmentMode{ EffectSpriteDirectionalAlignmentMode::TextureAxis };
    EffectSpriteTextureAxis spriteTextureAxis{ EffectSpriteTextureAxis::Y };
    float spriteRollOffsetDegrees{ 0.f };
};

struct EffectEmitterPlaybackRuntimeDesc
{
    float duration{ 1.f };
    uint32 loopCount{ 0 };
    float delay{ 0.f };
    bool delayFirstLoopOnly{ false };
    bool killOnDeactivate{ false };
    bool killOnCompleted{ false };
};

struct EffectDrawLimitRuntimeDesc
{
    bool useMaxDrawCount{ true };
    uint32 maxDrawCount{ 500 };
};

struct EffectRequiredSortRuntimeDesc
{
    EffectSortPolicy sortPolicy{ EffectSortPolicy::None };
    int32 sortLayer{ 0 };
    float artistSortBias{ 0.f };
};

struct EffectRequiredRuntimeDesc
{
    EffectRequiredMaterialRuntimeDesc material{};
    EffectSpriteRenderRuntimeDesc spriteRender{};
    EffectEmitterPlaybackRuntimeDesc playback{};
    EffectDrawLimitRuntimeDesc drawLimit{};
    EffectRequiredSortRuntimeDesc sort{};
};

//## Module::CommonRuntime

struct EffectSpawnRuntimeDesc
{
    uint32 instanceCount{ 96 };
    Vec3 spawnRange{ 1.f, 1.f, 1.f };
    PointParticleSpawnDesc particleSpawn{};
};

struct EffectLifetimeRuntimeDesc
{
    Vec2 lifeTime{ 0.5f, 1.f };
    PointParticleFloatCurveRuntimeDesc lifeTimeCurve{};
};

struct EffectInitialSizeRuntimeDesc
{
    bool enabled{ false };
    Vec2 sizeMin{ 0.12f, 0.12f };
    Vec2 sizeMax{ 0.28f, 0.28f };
    PointParticleRandomSeedRuntimeDesc sizeSeed{};
};

struct EffectInitialColorRuntimeDesc
{
    Vec4 startColorMin{ 1.f, 1.f, 1.f, 1.f };
    Vec4 startColorMax{ 1.f, 1.f, 1.f, 1.f };
    PointParticleRandomSeedRuntimeDesc colorSeed{};
    PointParticleRandomSeedRuntimeDesc alphaSeed{};
};

struct EffectColorOverLifeRuntimeDesc
{
    Vec4 endColorMin{ 1.f, 1.f, 1.f, 1.f };
    Vec4 endColorMax{ 1.f, 1.f, 1.f, 1.f };
    PointParticleColorOverLifeCurveDesc curve{};
    PointParticleRandomSeedRuntimeDesc colorSeed{};
    PointParticleRandomSeedRuntimeDesc alphaSeed{};
};

struct EffectTrailSpawnPerUnitRuntimeDesc
{
    bool enabled{ false };
    float spawnPerUnit{ 0.f };
    float unitScalar{ 1.f };
    float movementTolerance{ 0.f };
    float maxFrameDistance{ 0.f };
};

//## Emitter::RuntimeDesc

struct ComputeSpriteEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    PointParticleDrawMode drawMode{
        PointParticleDrawMode::DrawIndexedInstancedIndirect };
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto };
    EffectRequiredRuntimeDesc required{};
    EffectSpawnRuntimeDesc spawn{};
    EffectLifetimeRuntimeDesc lifetime{};
    EffectInitialSizeRuntimeDesc initialSize{};
    EffectInitialColorRuntimeDesc initialColor{};
    EffectColorOverLifeRuntimeDesc colorOverLife{};
    PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{};
    PointParticleSizeByLifeDesc sizeByLife{};
    PointParticleMotionDesc motion{};
    EffectOrbitOverLifeRuntimeDesc orbitOverLife{};
    PointParticleRotationDesc rotation{};
    PointParticleSpriteTiltDesc spriteTilt{};
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};
    PointParticleInitialLocationDesc initialLocation{};
    PointParticleSphereLocationDesc sphereLocation{};
    PointParticlePlaneRadialLocationDesc planeRadialLocation{};
    PointParticleCylinderLocationDesc cylinderLocation{};
    PointParticlePlaneRadialOrientationDesc planeRadialOrientation{};
    PointParticleCylinderOrientationDesc cylinderOrientation{};
    PointParticleRandomSeedRuntimeDesc initialLocationSeed{};
    PointParticleRandomSeedRuntimeDesc sphereLocationSeed{};
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{};
    Vec3 center{ 0.f, 0.f, 0.f };
};

struct ComputeTrailEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto };
    EffectRequiredMaterialRuntimeDesc material{};
    EffectEmitterPlaybackRuntimeDesc playback{};
    EffectLifetimeRuntimeDesc lifetime{};
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};
    bool useLifetimeSegmentLifetime{ false };
    EffectInitialColorRuntimeDesc initialColor{};
    EffectColorOverLifeRuntimeDesc colorOverLife{};
    PointParticleSizeByLifeDesc sizeByLife{};
    EffectTrailSpawnPerUnitRuntimeDesc spawnPerUnit{};

    float width{ 1.f };
    float segmentLifetime{ 0.25f };
    uint32 historyCount{ 32 };
    float uvTiling{ 1.f };
    float maxTrailLength{ 0.f };
    float tailFadeLength{ 0.1f };
    bool autoLifeFade{ true };
    float sampleSpacing{ 0.08f };
    uint32 curveSubdivision{ 4 };
    bool smoothTangent{ true };
    float sideFade{ 0.08f };

    Weak<IEffectTrailSampleProvider> sampleProvider{};
};

enum class EffectRibbonRenderAxis : uint8
{
    CameraUp,
    ViewUp,
    WorldUp,
    SourceUp,
    SourceRight,
};

enum class EffectRibbonSpreadBasis : uint8
{
    CameraFacing,
    ViewUp,
    WorldUp,
    SourceUp,
    SourceRight,
};

enum class EffectBeamEndpointMode : uint8
{
    StartEnd,
    DirectionLength,
};

enum class EffectBeamBranchPreset : uint8
{
    EndGuided,
    DownStrike,
    Entangle,
    ShortCrack,
};

struct EffectBeamEnvelopeOverLifeRuntimeDesc
{
    bool enabled{ false };
    float startRatioFallback{ 0.f };
    float endRatioFallback{ 1.f };
    float widthScaleFallback{ 1.f };
    PointParticleFloatCurveRuntimeDesc startRatioOverLife{};
    PointParticleFloatCurveRuntimeDesc endRatioOverLife{};
    PointParticleFloatCurveRuntimeDesc widthScaleOverLife{};
};

struct ComputeBeamEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto };
    EffectRequiredMaterialRuntimeDesc material{};
    EffectEmitterPlaybackRuntimeDesc playback{};
    EffectLifetimeRuntimeDesc lifetime{};
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};
    bool useLifetimeVisualLife{ false };
    EffectRequiredSortRuntimeDesc sort{};
    EffectInitialColorRuntimeDesc initialColor{};
    EffectColorOverLifeRuntimeDesc colorOverLife{};
    PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{};
    PointParticleSizeByLifeDesc sizeByLife{};
    EffectBeamEnvelopeOverLifeRuntimeDesc beamEnvelopeOverLife{};
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{};
    bool usePerStripSubUVVariation{ false };

    EffectBeamEndpointMode endpointMode{
        EffectBeamEndpointMode::StartEnd };
    Vec3 localStart{ 0.f, 2.f, 0.f };
    Vec3 localEnd{ 0.f, -2.f, 0.f };
    Vec3 localDirection{ 0.f, -1.f, 0.f };
    float length{ 4.f };
    uint32 segmentCount{ 8 };
    float noiseAmplitude{ 0.f };
    uint32 seed{ 1 };
    uint32 stripCount{ 1 };
    float endSpreadRadius{ 0.f };
    float lengthVariance{ 0.f };
    EffectBeamBranchPreset branchPreset{
        EffectBeamBranchPreset::EndGuided };
    bool branchEnabled{ false };
    uint32 branchCount{ 0 };
    float branchChance{ 1.f };
    uint32 branchSegmentCount{ 3 };
    float branchLength{ 0.75f };
    float branchLengthVariance{ 0.25f };
    float branchStartMin{ 0.2f };
    float branchStartMax{ 0.85f };
    float branchSpreadRadius{ 0.5f };
    float branchEndSpreadRadius{ 0.5f };
    float branchOutwardAmount{ 0.25f };
    float branchCurveAmount{ 0.5f };
    float branchDownLength{ 1.f };
    float branchEntangleRadius{ 0.35f };
    float branchEntangleAdvance{ 0.25f };
    float branchCrackLength{ 0.5f };
    float branchCrackSpreadRadius{ 0.25f };
    float branchWidthScale{ 0.45f };
    uint32 branchSeedOffset{ 1009 };
    float baseWidth{ 0.25f };
    float tilingDistance{ 0.f };
};

enum class EffectSourceHistoryRibbonSourceMode : uint8
{
    SelfRoot,
    ParticleEmitter,
};

struct EffectFollowerSourcePoint
{
    uint32 sourceIndex{};
    Vec3 position{};
};

struct ComputeRibbonEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto };
    EffectRequiredMaterialRuntimeDesc material{};
    EffectEmitterPlaybackRuntimeDesc playback{};
    EffectLifetimeRuntimeDesc lifetime{};
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};
    bool useLifetimeSampleLifetime{ false };
    EffectRequiredSortRuntimeDesc sort{};
    EffectInitialColorRuntimeDesc initialColor{};
    EffectColorOverLifeRuntimeDesc colorOverLife{};
    PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{};
    PointParticleSizeByLifeDesc sizeByLife{};
    PointParticleInitialLocationDesc headOffset{};
    PointParticleRandomSeedRuntimeDesc headOffsetSeed{};
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{};

    EffectSourceHistoryRibbonSourceMode sourceMode{
        EffectSourceHistoryRibbonSourceMode::SelfRoot };

    uint32 sourceEmitterId{};
    uint32 followerLaneCount{ 1 };
    uint32 maxSampleCount{ 64 };
    float sampleSpacing{ 0.08f };
    uint32 curveSubdivision{ 4 };
    bool smoothTangent{ true };
    float sampleInterval{ 0.f };
    float sampleLifetime{ 0.35f };

    float maxLength{ 0.f };
    float tailFadeLength{ 0.f };

    bool laneSpawnFadeInEnabled{ true };
    float laneSpawnFadeInDuration{ 0.08f };

    float baseWidth{ 0.3f };
    float tilingDistance{ 0.f };
    bool autoLifeFade{ false };
    bool tailCollapseOnIdle{ true };
    float tailCollapseSpeed{ 10.f };
    EffectRibbonSpreadBasis spreadBasis{
        EffectRibbonSpreadBasis::CameraFacing };
    float spreadAngleDegrees{ 0.f };
    bool useManualRoll{ false };
    float manualRollDegrees{ 0.f };

    Weak<IEffectSourcePointSampleProvider> sourcePointSampleProvider{};
};

enum class EffectSourceHistorySpriteTrailStampSpawnMode : uint8
{
    Distance,
    Time,
};

enum class EffectSourceHistorySpriteTrailDirectionMode : uint8
{
    PathTangent,
    CameraFacing,
    VelocityTextureAxis,
};

enum class EffectSourceHistorySpriteTrailPathFollowDirection : uint8
{
    TowardHead,
    TowardTail,
};

enum class EffectSourceHistorySpriteTrailPathReplayMode : uint8
{
    RecordedSpeed,
    FitDuration,
};

enum class EffectSourceHistorySpriteTrailPathReplayStartMode : uint8
{
    TailFirst,
    AllAtOnce,
};

enum class EffectSourceHistorySpriteTrailArrivalMode : uint8
{
    KillOnArrive,
    ClampAtEnd,
};

struct EffectSourceHistorySpriteTrailPathFollowRuntimeDesc
{
    bool enabled{ false };
    EffectSourceHistorySpriteTrailPathFollowDirection direction{ EffectSourceHistorySpriteTrailPathFollowDirection::TowardHead };
    Vec2 speed{ 1.f, 1.f };
    PointParticleRandomSeedRuntimeDesc speedSeed{};
    Vec2 startDelay{ 0.f, 0.f };
    PointParticleRandomSeedRuntimeDesc startDelaySeed{};
    EffectSourceHistorySpriteTrailArrivalMode arrivalMode{ EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive };
};

struct EffectSourceHistorySpriteTrailPathReplayRuntimeDesc
{
    bool enabled{ false };
    float delayTime{ 0.f };
    EffectSourceHistorySpriteTrailPathReplayMode replayMode{ EffectSourceHistorySpriteTrailPathReplayMode::RecordedSpeed };
    float speedScale{ 1.f };
    float drainDuration{ 0.35f };
    PointParticleFloatCurveRuntimeDesc drainCurve{};
    EffectSourceHistorySpriteTrailPathReplayStartMode startMode{ EffectSourceHistorySpriteTrailPathReplayStartMode::TailFirst };
    EffectSourceHistorySpriteTrailArrivalMode arrivalMode{ EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive };
};

struct ComputeSourceHistorySpriteTrailEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{ EffectEmitterRenderLayerOverride::Auto };
    EffectRequiredMaterialRuntimeDesc material{};
    EffectEmitterPlaybackRuntimeDesc playback{};
    EffectDrawLimitRuntimeDesc drawLimit{};
    EffectLifetimeRuntimeDesc lifetime{};
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};
    bool useLifetimeStampLifetime{ false };
    EffectRequiredSortRuntimeDesc sort{};
    EffectInitialSizeRuntimeDesc initialSize{};
    EffectInitialColorRuntimeDesc initialColor{};
    EffectColorOverLifeRuntimeDesc colorOverLife{};
    PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{};
    PointParticleSizeByLifeDesc sizeByLife{};
    PointParticleMotionDesc motion{};
    PointParticleRotationDesc rotation{};
    PointParticleSpriteTiltDesc spriteTilt{};
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{};
    EffectSpriteRenderRuntimeDesc spriteRender{};

    EffectSourceHistoryRibbonSourceMode sourceMode{ EffectSourceHistoryRibbonSourceMode::SelfRoot };
    uint32 sourceEmitterId{};
    uint32 followerLaneCount{ 1 };
    float sampleLifetime{ 0.35f };
    float sampleSpacing{ 0.08f };
    uint32 curveSubdivision{ 4 };
    bool smoothTangent{ true };
    float maxLength{ 0.f };

    EffectSourceHistorySpriteTrailStampSpawnMode stampSpawnMode{ EffectSourceHistorySpriteTrailStampSpawnMode::Distance };
    float stampSpacing{ 0.12f };
    float stampInterval{ 0.03f };
    float stampLifetime{ 0.18f };
    uint32 maxStampCount{ 128 };
    float cardLength{ 0.45f };
    float cardWidth{ 0.08f };
    bool flipU{ false };
    bool flipV{ false };
    float rotationOffsetDegrees{ 0.f };
    float spawnJitter{ 0.f };
    EffectSourceHistorySpriteTrailPathFollowRuntimeDesc pathFollow{};
    EffectSourceHistorySpriteTrailPathReplayRuntimeDesc pathReplay{};

    Weak<IEffectSourcePointSampleProvider> sourcePointSampleProvider{};
};

struct MeshVector3CurveRuntimeDesc
{
    bool enabled{ false };
    Vec3 start{ 1.f, 1.f, 1.f };
    Vec3 end{ 1.f, 1.f, 1.f };
    uint32 curveKeyCount{ 2 };
    Vec4 curveKeyTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 curveKeyTimesBlock1{};
    Vec4 curveKeyValuesX{ 1.f, 1.f, 0.f, 0.f };
    Vec4 curveKeyValuesXBlock1{};
    Vec4 curveKeyValuesY{ 1.f, 1.f, 0.f, 0.f };
    Vec4 curveKeyValuesYBlock1{};
    Vec4 curveKeyValuesZ{ 1.f, 1.f, 0.f, 0.f };
    Vec4 curveKeyValuesZBlock1{};
    Vec4 curveKeyArriveTangentsX{};
    Vec4 curveKeyArriveTangentsXBlock1{};
    Vec4 curveKeyLeaveTangentsX{};
    Vec4 curveKeyLeaveTangentsXBlock1{};
    Vec4 curveKeyArriveTangentsY{};
    Vec4 curveKeyArriveTangentsYBlock1{};
    Vec4 curveKeyLeaveTangentsY{};
    Vec4 curveKeyLeaveTangentsYBlock1{};
    Vec4 curveKeyArriveTangentsZ{};
    Vec4 curveKeyArriveTangentsZBlock1{};
    Vec4 curveKeyLeaveTangentsZ{};
    Vec4 curveKeyLeaveTangentsZBlock1{};
    Vec4 curveKeyModes{ 1.f, 1.f, 0.f, 0.f };
    Vec4 curveKeyModesBlock1{};
};

struct MeshTransformRuntimeDesc
{
    EffectScreenAlignment alignment{ EffectScreenAlignment::FacingCameraPosition };

    bool initialScaleEnabled{ false };
    Vec3 initialScaleMin{ 0.12f, 0.12f, 0.12f };
    Vec3 initialScaleMax{ 0.28f, 0.28f, 0.28f };
    PointParticleRandomSeedRuntimeDesc initialScaleSeed{};
    MeshVector3CurveRuntimeDesc scaleByLife{};

    bool rotationEnabled{ false };
    Vec3 initialRotationDegreesMin{ 0.f, 0.f, 0.f };
    Vec3 initialRotationDegreesMax{ 0.f, 0.f, 0.f };
    PointParticleRandomSeedRuntimeDesc initialRotationSeed{};
    MeshVector3CurveRuntimeDesc rotationByLife{};
    Vec3 initialAngularVelocityDegreesMin{ 0.f, 0.f, 0.f };
    Vec3 initialAngularVelocityDegreesMax{ 0.f, 0.f, 0.f };
    bool initialAngularVelocityInWorldSpace{ false };
    PointParticleRandomSeedRuntimeDesc initialAngularVelocitySeed{};
    MeshVector3CurveRuntimeDesc angularVelocityScaleByLife{};
};

enum class EffectMeshDirectionAlignTargetMode : uint8
{
    Direction,
    Point,
};

enum class EffectMeshDirectionAlignSpace : uint8
{
    Local,
    World,
};

enum class EffectMeshDirectionAlignBlendMode : uint8
{
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
};

struct MeshDirectionAlignRuntimeDesc
{
    bool enabled{ false };
    EffectMeshDirectionAlignTargetMode targetMode{ EffectMeshDirectionAlignTargetMode::Direction };
    EffectMeshDirectionAlignSpace space{ EffectMeshDirectionAlignSpace::World };
    Vec3 target{ 0.f, 0.f, 1.f };
    PointParticlePlaneRadialOrientationAxis meshForwardAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveZ };
    PointParticlePlaneRadialOrientationAxis meshUpAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveY };
    PointParticleFloatCurveRuntimeDesc alignmentProgress{};
    Vec2 randomDelay{ 0.f, 0.f };
    PointParticleRandomSeedRuntimeDesc randomDelaySeed{};
    Vec2 randomWeightScale{ 1.f, 1.f };
    PointParticleRandomSeedRuntimeDesc randomWeightScaleSeed{};
    EffectMeshDirectionAlignBlendMode blendMode{ EffectMeshDirectionAlignBlendMode::Linear };
};

struct MeshEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto };
    string modelGuid{};
    string modelPath{};
    Vec3 previewScale{ 1.f, 1.f, 1.f };
    bool useModelMaterials{ false };
    EffectRequiredMaterialRuntimeDesc material{};
    EffectEmitterPlaybackRuntimeDesc playback{};
    EffectDrawLimitRuntimeDesc drawLimit{};
    EffectRequiredSortRuntimeDesc sort{};
    EffectSpawnRuntimeDesc spawn{};
    EffectLifetimeRuntimeDesc lifetime{};
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};
    EffectInitialColorRuntimeDesc initialColor{};
    EffectColorOverLifeRuntimeDesc colorOverLife{};
    PointParticleMotionDesc motion{};
    EffectOrbitOverLifeRuntimeDesc orbitOverLife{};
    PointParticleInitialLocationDesc initialLocation{};
    PointParticleRandomSeedRuntimeDesc initialLocationSeed{};
    PointParticleSphereLocationDesc sphereLocation{};
    PointParticleRandomSeedRuntimeDesc sphereLocationSeed{};
    PointParticlePlaneRadialLocationDesc planeRadialLocation{};
    PointParticleCylinderLocationDesc cylinderLocation{};
    PointParticleSphereRadialOrientationDesc sphereRadialOrientation{};
    PointParticlePlaneRadialOrientationDesc planeRadialOrientation{};
    PointParticleCylinderOrientationDesc cylinderOrientation{};
    MeshTransformRuntimeDesc meshTransform{};
    MeshDirectionAlignRuntimeDesc meshDirectionAlign{};
};

using EffectEmitterConcreteDesc = variant<
    ComputeSpriteEmitterDesc,
    ComputeTrailEmitterDesc,
    ComputeRibbonEmitterDesc,
    ComputeSourceHistorySpriteTrailEmitterDesc,
    ComputeBeamEmitterDesc,
    MeshEmitterDesc>;

struct EffectEmitterDefinition
{
    uint32 id{};
    string name{};
    EffectEmitterKind kind{ EffectEmitterKind::Sprite };
    bool enabled{ true };
    EffectEmitterHistoryBudgetRuntimeDesc historyBudget{};
    EffectHistoryBudgetEffectiveDesc effectiveHistoryBudget{};

    Vec3 localPosition{ 0.f, 0.f, 0.f };
    Vec3 localRotationDegrees{ 0.f, 0.f, 0.f };
    bool useLocalSpace{ false };
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto };

    EffectEmitterConcreteDesc concreteDesc{
        ComputeSpriteEmitterDesc{} };
};

//## Effect::RuntimeDesc

struct EffectDefinition
{
    string name{};
    EffectHistoryBudgetRuntimeDesc historyBudget{};
    vector<EffectHistorySourceGroupBudgetDesc> historySourceGroups{};
    vector<EffectEmitterDefinition> emitters{};
};

struct EffectInstanceDesc : public GameObject::GAMEOBJECT_DESC
{
    Shared<const EffectDefinition> definition{};
};

NS_END
