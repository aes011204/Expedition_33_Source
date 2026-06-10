#include "EffectAuthoringValueRange.h"

NS_BEGIN(EffectEditor)

namespace
{
    struct AuthoringValueRangeEntry
    {
        AuthoringModuleType moduleType{};
        std::string_view fieldId{};
        AuthoringValueRange range{};
    };

    constexpr AuthoringValueRange MinOnly(float minValue)
    {
        return AuthoringValueRange{ true, false, minValue, 0.f };
    }

    constexpr AuthoringValueRange MinMax(float minValue, float maxValue)
    {
        return AuthoringValueRange{ true, true, minValue, maxValue };
    }

    constexpr AuthoringValueRangeEntry kAuthoringValueRanges[] = {
        // EffectAssetRuntimeLoader clamps playback duration to a positive minimum.
        { AuthoringModuleType::Required, "duration", MinOnly(0.0001f) },
        // EffectAssetRuntimeLoader clamps playback delay to non-negative.
        { AuthoringModuleType::Required, "delay", MinOnly(0.f) },
        // EffectAssetRuntimeLoader consumes spawn rate distributions as non-negative ranges.
        { AuthoringModuleType::Spawn, "spawnRate", MinOnly(0.f) },
        // EffectAssetRuntimeLoader consumes spawn rate scale distributions as non-negative ranges.
        { AuthoringModuleType::Spawn, "spawnRateScale", MinOnly(0.f) },
        // EffectAssetRuntimeLoader consumes burst scale distributions as non-negative ranges.
        { AuthoringModuleType::Spawn, "burstScale", MinOnly(0.f) },
        // Particle lifetime must stay positive for over-life evaluation.
        { AuthoringModuleType::Lifetime, "lifeTime", MinOnly(0.0001f) },
        // EffectAssetRuntimeLoader clamps sphere radius to non-negative.
        { AuthoringModuleType::SphereLocation, "radius", MinOnly(0.f) },
        // EffectAssetRuntimeLoader consumes radial radius distributions as non-negative ranges.
        { AuthoringModuleType::PlaneRadialLocation, "radiusDistribution", MinOnly(0.f) },
        // EffectAssetRuntimeLoader consumes drag distributions as non-negative ranges.
        { AuthoringModuleType::Drag, "drag", MinOnly(0.f) },
        // EffectAssetRuntimeLoader clamps authored alpha distributions to 0..1.
        { AuthoringModuleType::InitialColor, "alpha", MinMax(0.f, 1.f) },
        // EffectAssetRuntimeLoader clamps over-life alpha endpoints to 0..1.
        { AuthoringModuleType::ColorOverLife, "alphaOverLife", MinMax(0.f, 1.f) },
        // Frame indices have no negative authoring meaning; atlas dynamic max is a later phase.
        { AuthoringModuleType::SubUVFrameOverLife, "frameIndex", MinOnly(0.f) },
    };
}

const AuthoringValueRange* Find_AuthoringValueRange(AuthoringModuleType moduleType, std::string_view fieldId)
{
    for (const AuthoringValueRangeEntry& entry : kAuthoringValueRanges)
    {
        if (entry.moduleType == moduleType && entry.fieldId == fieldId)
            return &entry.range;
    }

    return nullptr;
}

NS_END
