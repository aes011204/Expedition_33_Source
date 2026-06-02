#pragma once

#include "EffectAuthoring_Types.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(EffectEditor)

struct MaterialScalarModulationPreviewResult
{
    EffectMaterialInstanceData material{};
    bool applied{ false };
    bool skippedParticleLife{ false };
};

const RequiredModuleData* Find_RequiredModuleData(const AuthoringEmitter& emitter);
const MaterialScalarModulationModuleData* Find_MaterialScalarModulationModuleData(const AuthoringEmitter& emitter);
EffectMaterialScalarModulationRuntimeDesc Build_MaterialScalarModulationRuntimeDesc(const MaterialScalarModulationModuleData& data);
float Compute_MaterialScalarPreviewPhase(float elapsedTime, float duration);
MaterialScalarModulationPreviewResult Evaluate_MaterialScalarModulationPreview(
    const EffectMaterialInstanceData& baseMaterial,
    const EffectMaterialScalarModulationRuntimeDesc& modulation,
    float previewPhase,
    bool evaluateParticleLife);

NS_END
