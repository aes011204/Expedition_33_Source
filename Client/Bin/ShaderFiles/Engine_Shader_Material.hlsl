#ifndef ENGINE_SHADER_MATERIAL_HLSLI
#define ENGINE_SHADER_MATERIAL_HLSLI

#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"

//==================================================
// MATERIAL TEXTURES
//==================================================
Texture2D g_MaterialTextures[MATERIAL_SLOT_END];

//==================================================
// 컴파일 단계에서 inline 최적화 진행됨
//==================================================
bool HasMaterialTexture(uint iSlot)
{
    return (g_iMaterialMask & (1u << iSlot)) != 0;
}

//==================================================
// UV
//==================================================
float2 ApplyUVTransform(float2 uv)
{
    return uv * g_UVScale + g_UVOffset;
}

//==================================================
// SAMPLE FUNCTIONS
//==================================================
float4 SampleBaseColor(float2 uv)
{
    float4 baseColor = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, uv);

    if (baseColor.a < 0.33)
        discard;

    baseColor.rgb *= g_BaseColorTint.rgb;
    baseColor.a *= g_BaseColorTint.a;
    return baseColor;
}

float GetOpacityMaskForMasked(float2 uv, float4 baseColor)
{
    if (HasMaterialTexture(MATERIAL_SLOT_OPACITY_MASK))
        return g_MaterialTextures[MATERIAL_SLOT_OPACITY_MASK].Sample(LinearWrapSampler, uv).r;

    // Masked인데 마스크 텍스처가 없으면 BaseColor.a는 패킹 채널(ORM/마스크 등)인 경우가 많아서
    // 예기치 않게 거의 전부 discard 되는 문제가 생길 수 있다.
    // 따라서 기본값은 "마스크 없음 = 전부 통과"로 둔다.
    return 1.f;
}

float3 ComputeWorldNormal(float3 normalW, float3 tangentW, float3 binormalW, float2 uv)
{
    float3 n = normalize(normalW);

    if (!HasMaterialTexture(MATERIAL_SLOT_NORMAL))
        return n;

    float3 t = normalize(tangentW);
    float3 b = normalize(binormalW);

    float3 normalTS = g_MaterialTextures[MATERIAL_SLOT_NORMAL].Sample(LinearWrapSampler, uv).xyz * 2.f - 1.f;
    normalTS.xy *= g_NormalStrength;

    return normalize(normalTS.x * t + normalTS.y * b + normalTS.z * n);
}

float3 SampleOrm(float2 uv)
{
    float3 orm = float3(1.f, 1.f, 1.f);

    if (HasMaterialTexture(MATERIAL_SLOT_ORM))
        orm = g_MaterialTextures[MATERIAL_SLOT_ORM].Sample(LinearWrapSampler, uv).rgb;

    orm.r *= g_AoScale;         // O
    orm.g *= g_RoughnessScale;  // R
    orm.b *= g_MetalnessScale;  // M

    return orm;
}

float3 SampleEmissive(float2 uv)
{
    if (!HasMaterialTexture(MATERIAL_SLOT_EMISSIVE))
        return float3(0.f, 0.f, 0.f);

    float3 emissiveTex = g_MaterialTextures[MATERIAL_SLOT_EMISSIVE].Sample(LinearWrapSampler, uv).rgb;

    float3 emissive = emissiveTex * g_EmissiveColor.rgb * g_EmissiveIntensity;
    emissive += emissiveTex * g_EmissiveMaskMul;

    return emissive;
}

#endif
