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
    
    if (baseColor.a < 0.34f)
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

struct EyeMaterialSample
{
    float3  baseColor;
    float   opacity;
};

EyeMaterialSample SampleEyeMaterial(float2 uv)
{
    EyeMaterialSample Out;

    float3 baseColorTex = float3(0.88f, 0.90f, 0.92f);
    if (HasMaterialTexture(MATERIAL_SLOT_BASECOLOR))
    {
        baseColorTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, uv).rgb;
    }

    const bool hasIrisHeightTex = HasMaterialTexture(MATERIAL_SLOT_IRIS_HEIGHT);

    float irisHeight = 0.5f;
    if (hasIrisHeightTex)
        irisHeight = g_MaterialTextures[MATERIAL_SLOT_IRIS_HEIGHT].Sample(LinearWrapSampler, uv).r;

    // Iris 영역은 UV 기준 원형으로 고정한다.
    // (Iris_M 텍스처는 채널 packed 성격이 있어 영역 마스크로 쓰면 중심 점만 남는 문제가 발생)
    float2 eyeUV = uv * 2.f - 1.f;
    float radial = length(eyeUV);
    float irisArea = 1.f - smoothstep(0.20f, 0.46f, radial);
    irisArea = saturate(irisArea);

    if (!hasIrisHeightTex)
        irisHeight = 1.f - smoothstep(0.06f, 0.34f, radial);

    // 홍채 tint는 기존 BaseColorTint를 재사용하되, 지나친 검정화 방지를 위해 강도를 제한한다.
    float3 irisTint = saturate(g_BaseColorTint.rgb);
    if (irisTint.r > 0.99f && irisTint.g > 0.99f && irisTint.b > 0.99f)
    {
        irisTint = float3(0.30f, 0.22f, 0.14f);
    }

    float fiber = saturate(irisHeight);
    float limbal = smoothstep(0.28f, 0.46f, radial);
    float pupilMask = 1.f - smoothstep(0.06f, 0.13f, radial);

    float3 scleraColor = saturate(baseColorTex * 1.02f);
    float3 irisBaseColor = lerp(float3(0.10f, 0.08f, 0.06f), irisTint, lerp(0.35f, 0.95f, fiber));
    irisBaseColor *= lerp(1.0f, 0.55f, limbal); // 홍채 바깥 고리 살짝 어둡게
    float3 irisColor = irisBaseColor;
    // 중심 동공은 반드시 어둡게 고정한다.
    irisColor = lerp(irisColor, float3(0.015f, 0.015f, 0.02f), pupilMask);
    irisColor *= lerp(0.82f, 1.0f, irisArea);

    // 홍채-흰자 전이를 더 급하게 만들어 탁한 중간톤 띠를 줄인다.
    float irisBlend = smoothstep(0.32f, 0.88f, irisArea);
    irisBlend = saturate(pow(irisBlend, 0.55f));

    Out.baseColor = lerp(scleraColor, irisColor, irisBlend);
    Out.baseColor = saturate(Out.baseColor);

    Out.opacity = 1.f;

    return Out;
}


#endif
