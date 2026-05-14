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
    float ao = 1.f;         // AO 는 가리지 않고
    float roughness = 1.f;  // Rougness 는 거칠게
    float metalness = 0.f;  // Metalness는 비금속

    if (HasMaterialTexture(MATERIAL_SLOT_ORM))
    {
        float3 orm = g_MaterialTextures[MATERIAL_SLOT_ORM].Sample(LinearWrapSampler, uv).rgb;

        ao = orm.r;
        roughness = orm.g;
        metalness = orm.b;
    }

    ao *= g_AoScale;                // O
    roughness *= g_RoughnessScale;  // R
    metalness *= g_MetalnessScale;  // M

    return float3(ao, roughness, metalness);
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

struct HairMaterialSample
{
    float3  baseColor;  // 최종 diffuse
    float   opacity;    // 마스크
    float   depth;      // hair card 내부 깊이/겹침 보정값
    float   id;         // 가닥/영역별 variation 값
};

HairMaterialSample SampleHairMaterial(float2 uv)
{
    HairMaterialSample Out;

    // Color
    float4 colorTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, uv);

    // Mask
    float opacity = colorTex.a;
    if (HasMaterialTexture(MATERIAL_SLOT_OPACITY_MASK))
    {
        opacity = g_MaterialTextures[MATERIAL_SLOT_OPACITY_MASK].Sample(LinearWrapSampler, uv).r;
    }

    // Depth
    // 일단은 PixelDepthOffset으로 쓰지않고, fake occlusion 값으로 사용
    float depth = 0.5f;
    if (HasMaterialTexture(MATERIAL_SLOT_HAIR_DEPTH))
    {
        depth = g_MaterialTextures[MATERIAL_SLOT_HAIR_DEPTH].Sample(LinearWrapSampler, uv).r;
    }

    // ID / Unique Hair Value
    // 가닥마다 색이 완전히 균일해 보이지 않게 variation을 만드는
    float id = 0.5f;
    if (HasMaterialTexture(MATERIAL_SLOT_HAIR_ID))
    {
        id = g_MaterialTextures[MATERIAL_SLOT_HAIR_ID].Sample(LinearWrapSampler, uv).r;
    }

    // 간단한 Lune hair 테스트용 shading
    float depthShade = lerp(0.82f, 1.08f, depth);
    float idVaration = (id - 0.5f) * 0.18f;

    Out.baseColor = saturate(colorTex.rgb * g_BaseColorTint.rgb * depthShade + idVaration);
    Out.opacity = opacity;
    Out.depth = depth;
    Out.id = id;

    return Out;
}

#endif
