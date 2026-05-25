#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"

cbuffer cbEffectMaterialPreview : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMaterialPreviewParams = { 1.f, 1.f, 0.f, 0.f };        // x: intensity, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectMaterialPreviewAlphaParams = { 0.f, 0.f, 0.f, 0.f };   // x: alphaCutoff, y: alphaErosion, z: useMask, w: reserved
    float4 g_EffectMaterialPreviewMainUVParams = { 1.f, 1.f, 0.f, 0.f };  // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewNoiseUVParams = { 1.f, 1.f, 0.f, 0.f }; // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewMaskUVParams = { 1.f, 1.f, 0.f, 0.f };  // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewUVModeParams = { 0.f, 0.f, 0.f, 0.f };  // x: main, y: noise, z: mask, w: reserved. 0 Wrap, 1 Stretch, 2 Clamp, 3 Raw
    float g_EffectMaterialPreviewTime = 0.f;
    int g_OpacitySource = 0;
    float2 g_EffectMaterialPreviewPadding;
};

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);

// preview 재질의 UV 스케일과 시간 기반 스크롤을 적용한다.
float2 Apply_MaterialPreviewUV(float2 uv, float4 uvParams)
{
    return uv * uvParams.xy + uvParams.zw * g_EffectMaterialPreviewTime;
}

bool Use_PreviewClampSampler(float mode)
{
    return mode >= 0.5f && mode < 2.5f;
}

float4 Sample_PreviewMainTexture(float2 uv, float mode)
{
    return Use_PreviewClampSampler(mode)
        ? g_Texture.Sample(LinearClampSampler, uv)
        : g_Texture.Sample(LinearWrapSampler, uv);
}

float Sample_PreviewNoiseTexture(float2 uv, float mode)
{
    return Use_PreviewClampSampler(mode)
        ? g_NoiseTexture.Sample(LinearClampSampler, uv).r
        : g_NoiseTexture.Sample(LinearWrapSampler, uv).r;
}

float Sample_PreviewMaskTexture(float2 uv, float mode)
{
    return Use_PreviewClampSampler(mode)
        ? g_MaskTexture.Sample(LinearClampSampler, uv).a
        : g_MaskTexture.Sample(LinearWrapSampler, uv).a;
}

struct VS_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    const float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    const float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.position = mul(float4(In.position, 1.f), matWVP);
    Out.normal = normalize(mul(float4(In.normal, 0.f), g_WorldMatrix).xyz);
    Out.texCoord = In.texCoord;

    return Out;
}

struct PS_IN
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float2 mainUV = Apply_MaterialPreviewUV(In.texCoord, g_EffectMaterialPreviewMainUVParams);
    const float2 noiseUV = Apply_MaterialPreviewUV(In.texCoord, g_EffectMaterialPreviewNoiseUVParams);
    const float2 maskUV = Apply_MaterialPreviewUV(In.texCoord, g_EffectMaterialPreviewMaskUVParams);
    const float mainUVMode = g_EffectMaterialPreviewUVModeParams.x;
    const float noiseUVMode = g_EffectMaterialPreviewUVModeParams.y;
    const float maskUVMode = g_EffectMaterialPreviewUVModeParams.z;
    const float4 symbol = Sample_PreviewMainTexture(mainUV, mainUVMode);
    const float opacityPower = max(g_EffectMaterialPreviewParams.y, 0.0001f);
    const float alphaCutoff = saturate(g_EffectMaterialPreviewAlphaParams.x);
    const float alphaErosion = saturate(g_EffectMaterialPreviewAlphaParams.y);
    float selectedOpacity = symbol.a;
    float noiseRaw = 1.f;
    float maskAlpha = 1.f;

    if (g_OpacitySource == 1)
        selectedOpacity = symbol.r;
    else if (2 == g_OpacitySource)
        selectedOpacity = dot(symbol.rgb, float3(0.299f, 0.587f, 0.114f));

    const float noiseStrength = saturate(g_EffectMaterialPreviewParams.z);
    if (g_EffectMaterialPreviewParams.w != 0 && g_EffectMaterialPreviewParams.z > 0.f)
        noiseRaw = Sample_PreviewNoiseTexture(noiseUV, noiseUVMode);

    if (g_EffectMaterialPreviewAlphaParams.z != 0)
        maskAlpha = Sample_PreviewMaskTexture(maskUV, maskUVMode);

    const float breakupNoise = lerp(1.f, noiseRaw, noiseStrength);
    float coverage = maskAlpha;

    if (alphaErosion > 0.f)
    {
        const float erosionSource = saturate(maskAlpha * breakupNoise);
        const float erosionFactor = smoothstep(alphaErosion, 1.f, erosionSource);
        coverage *= erosionFactor;
    }

    const float opacity = pow(saturate(selectedOpacity), opacityPower);
    const float energyVariation = lerp(1.f, lerp(0.75f, 1.25f, noiseRaw), noiseStrength);
    const float facing = saturate(abs(In.normal.z) * 0.35f + 0.65f);
    Out.color.rgb = symbol.rgb * g_Tint.rgb * g_EffectMaterialPreviewParams.x * energyVariation * facing;
    Out.color.a = opacity * coverage * breakupNoise * g_Tint.a;

    if (Out.color.a < max(alphaCutoff, 0.01f))
        discard;

    return Out;
}

technique11 DefaultTechnique
{
    // Plane preview
    PASS_RS_DS_BS_VP(AlphaBlendPass, RS_CullNone, DSS_Default, BS_AlphaBlend, VS_MAIN, PS_MAIN) // 0
    PASS_RS_DS_BS_VP(AdditivePass, RS_CullNone, DSS_Default, BS_Blend, VS_MAIN, PS_MAIN)        // 1

    // Sphere preview
    PASS_RS_DS_BS_VP(SphereAlphaBlendPass, RS_Default, DSS_Default, BS_AlphaBlend, VS_MAIN, PS_MAIN) // 2
    PASS_RS_DS_BS_VP(SphereAdditivePass, RS_Default, DSS_Default, BS_Blend, VS_MAIN, PS_MAIN)        // 3
}
