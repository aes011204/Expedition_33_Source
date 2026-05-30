#pragma once

#include "Client_Defines.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Client)

// .effect.json authoring source를 Client runtime EffectDefinition으로 접는 loader다.
class EffectAssetRuntimeLoader
{
public:
    EffectAssetRuntimeLoader() = delete;

public:
    // 리소스 루트 기준 .effect.json 경로를 EffectDefinition으로 변환한다.
    static Shared<const EffectDefinition> Load_Definition(const wstring& effectAssetPath);

    // Effects/Assets 아래의 .effect.json 파일 이름으로 EffectDefinition을 로드한다.
    static Shared<const EffectDefinition> Load_DefinitionByName(const wstring& effectName);

private:
    static const json* Find_ModuleData(const json& emitter, const string& moduleType);
    static float Evaluate_FloatCurveSegmentLinear(const json& leftKey, const json& rightKey, float x);
    static float Evaluate_FloatCurveSegmentAutoClamped(const json& leftKey, const json& rightKey, float x);
    static float Evaluate_FloatConstantCurve(const json& keys, float x, float fallbackValue);
    static Vec2 Evaluate_Vector2CurveSegmentLinear(const json& leftKey, const json& rightKey, float x);
    static Vec2 Evaluate_Vector2CurveSegmentAutoClamped(const json& leftKey, const json& rightKey, float x);
    static Vec2 Evaluate_Vector2ConstantCurve(const json& keys, float x, const Vec2& fallbackValue);
    static Vec3 Evaluate_Vector3ConstantCurve(const json& keys, float x, const Vec3& fallbackValue);
    static Vec4 Evaluate_ColorConstantCurve(const json& keys, float x, const Vec4& fallbackValue);
    static const json* Find_DistributionPayload(const json& distribution);
    static float Evaluate_FloatDistributionMin(const json& distribution, float fallbackValue);
    static float Evaluate_FloatDistributionMax(const json& distribution, float fallbackValue);
    static Vec2 Evaluate_Vector2DistributionMin(const json& distribution, const Vec2& fallbackValue);
    static Vec2 Evaluate_Vector2DistributionMax(const json& distribution, const Vec2& fallbackValue);
    static Vec3 Evaluate_Vector3DistributionMin(const json& distribution, const Vec3& fallbackValue);
    static Vec3 Evaluate_Vector3DistributionMax(const json& distribution, const Vec3& fallbackValue);
    static Vec4 Clamp_Color(Vec4 value);
    static Vec4 Evaluate_ColorDistributionMin(const json& distribution, const Vec4& fallbackValue);
    static Vec4 Evaluate_ColorDistributionMax(const json& distribution, const Vec4& fallbackValue);
    static void Set_SubUVFrameCurveComponent(Vec4& values, uint32 index, float value);
    static void Fill_SubUVFrameCurvePayload(PointParticleSubUVFrameOverLifeDesc& desc, const json& frameIndex);
    static void Fill_SizeByLifePayload(PointParticleSizeByLifeDesc& desc, const json& sizeByLife);
    static void Fill_RotationOverLifePayload(PointParticleRotationDesc& desc, const json& rotationOverLife);
    static string Resolve_RendererType(const json& emitter);
    static void Apply_MaterialPayload(
        const json& material,
        EffectRequiredMaterialRuntimeDesc& materialDesc,
        bool includeTrailMaterialFields);
    static void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeSpriteEmitterDesc& desc);
    static void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeTrailEmitterDesc& desc);
    static void Apply_SpawnModule(const json& data, ComputeSpriteEmitterDesc& desc);
    static bool Build_EmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition);
    static bool Build_TrailEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition);
    static bool Try_MakeResourceRelativePath(
        const fs::path& fullPath,
        const char* logContext,
        const string& assetKey,
        wstring& outRelativePath);
    static bool Resolve_EffectAssetPathByGuid(const string& effectAssetGuid, wstring& outEffectAssetPath);
    static string Normalize_EffectNameKey(const wstring& effectName);
    static bool Is_EffectDefinitionAssetPath(const wstring& relativePath);
    static bool Find_EffectAssetGuidByName(const wstring& effectName, string& outEffectAssetGuid);
    static bool Load_JsonRoot(const wstring& effectAssetPath, json& outRoot);
};

NS_END
