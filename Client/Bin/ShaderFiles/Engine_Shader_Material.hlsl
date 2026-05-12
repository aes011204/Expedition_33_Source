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
    float2 scaled = uv * g_UVScale;

    float s = sin(g_UVRotate);
    float c = cos(g_UVRotate);

    float2 rotated;
    rotated.x = scaled.x * c - scaled.y * s;
    rotated.y = scaled.x * s + scaled.y * c;

    return rotated + g_UVOffset;
}

//==================================================
// SAMPLE FUNCTIONS
//==================================================
float4 SampleBaseColor(float2 uv)
{
    float4 baseColor = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, uv);

    if (baseColor.a < 0.34)
        discard;

    baseColor.rgb *= g_BaseColorTint.rgb;
    baseColor.a *= g_BaseColorTint.a;
    return baseColor;
}

float GetOpacityMaskForMasked(float2 uv, float4 baseColor)
{
    if (HasMaterialTexture(MATERIAL_SLOT_OPACITY_MASK))
        return g_MaterialTextures[MATERIAL_SLOT_OPACITY_MASK].Sample(LinearWrapSampler, uv).r;

    return 1.f;
}

float3 ComputeWorldNormal(float3 normalW, float3 tangentW, float3 binormalW, float2 uv)
{
    float3 n = normalize(normalW);
    float3 t = normalize(tangentW);
    float3 b = normalize(binormalW);

    if (!HasMaterialTexture(MATERIAL_SLOT_NORMAL))
        return n;

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

    float emissiveMask = g_MaterialTextures[MATERIAL_SLOT_EMISSIVE]
        .Sample(LinearWrapSampler, uv).r;

    float3 emissive = g_EmissiveColor.rgb * emissiveMask * g_EmissiveIntensity;

    return emissive;
}

#endif
