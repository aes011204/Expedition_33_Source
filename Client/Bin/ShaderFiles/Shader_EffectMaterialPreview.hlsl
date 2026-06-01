#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"

cbuffer cbEffectMaterialPreview : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMaterialPreviewParams = { 1.f, 1.f, 0.f, 0.f };             // x: authored energy scalar, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectMaterialPreviewAlphaParams = { 0.f, 0.f, 0.f, 0.f };        // x: alphaCutoff, y: alphaErosion, z: useMask, w: reserved
    float4 g_EffectMaterialPreviewMainUVParams = { 1.f, 1.f, 0.f, 0.f };       // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };      // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewMaskUVParams = { 1.f, 1.f, 0.f, 0.f };       // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };     // xy: main offset, zw: noise offset
    float4 g_EffectMaterialPreviewMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f }; // xy: mask offset, zw: reserved
    float4 g_EffectMaterialPreviewUVModeParams = { 0.f, 0.f, 0.f, 0.f };       // x: main, y: noise, z: mask, w: reserved. 0 Wrap, 1 Stretch, 2 Clamp, 3 Raw
    float4 g_EffectMaterialPreviewAdditiveParams = { 0.f, 0.f, 0.f, 1.f };     // x: colorSource, y: amountSource, z: coveragePolicy, w: intensityScale
    float4 g_EffectMaterialPreviewAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMaterialPreviewAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMaterialPreviewAdditiveFlags = { 1.f, 0.f, 0.f, 0.f };         // x: blackNeutral
    float4 g_EffectMaterialPreviewCoreEmissiveParams = { 0.f, 4.f, 2.f, 1.f };    // x: enabled, y: corePower, z: coreIntensity, w: outerPower
    float4 g_EffectMaterialPreviewCoreEmissiveColor = { 1.f, 0.85f, 0.45f, 1.f }; // rgb: coreColor, a: outerIntensity
    float4 g_EffectMaterialPreviewCheckerParams = { 512.f, 512.f, 0.f, 0.f };     // xy: target size, z: time, w: reserved
    float g_EffectMaterialPreviewTime = 0.f;
    int g_OpacitySource = 0;
    float2 g_EffectMaterialPreviewPadding;
};

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);

// preview 재질의 UV 스케일과 시간 기반 스크롤을 적용한다.
float2 Apply_MaterialPreviewUV(float2 uv, float4 uvParams, float2 uvOffset)
{
    return uv * uvParams.xy + uvOffset + uvParams.zw * g_EffectMaterialPreviewTime;
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

VS_OUT VS_CHECKER(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    Out.position = float4(In.position.xy, 0.5f, 1.f);
    Out.normal = float3(0.f, 0.f, -1.f);
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

EffectMaterialSurface Resolve_EffectMaterialSurface(PS_IN In)
{
    EffectMaterialSurface surface = (EffectMaterialSurface)0;

    const float2 mainUV = Apply_MaterialPreviewUV(In.texCoord, g_EffectMaterialPreviewMainUVParams, g_EffectMaterialPreviewUVOffsetParams.xy);
    const float2 noiseUV = Apply_MaterialPreviewUV(In.texCoord, g_EffectMaterialPreviewNoiseUVParams, g_EffectMaterialPreviewUVOffsetParams.zw);
    const float2 maskUV = Apply_MaterialPreviewUV(In.texCoord, g_EffectMaterialPreviewMaskUVParams, g_EffectMaterialPreviewMaskUVOffsetParams.xy);
    const float mainUVMode = g_EffectMaterialPreviewUVModeParams.x;
    const float noiseUVMode = g_EffectMaterialPreviewUVModeParams.y;
    const float maskUVMode = g_EffectMaterialPreviewUVModeParams.z;
    const float4 symbol = Sample_PreviewMainTexture(mainUV, mainUVMode);
    const float opacityPower = max(g_EffectMaterialPreviewParams.y, 0.0001f);
    const float alphaCutoff = saturate(g_EffectMaterialPreviewAlphaParams.x);
    const float alphaErosion = saturate(g_EffectMaterialPreviewAlphaParams.y);
    const float selectedOpacity = Effect_SelectOpacity(symbol, g_OpacitySource);
    float noiseRaw = 1.f;
    float maskAlpha = 1.f;

    const float noiseStrength = saturate(g_EffectMaterialPreviewParams.z);
    if (g_EffectMaterialPreviewParams.w != 0 && g_EffectMaterialPreviewParams.z > 0.f)
        noiseRaw = Sample_PreviewNoiseTexture(noiseUV, noiseUVMode);

    if (g_EffectMaterialPreviewAlphaParams.z != 0)
        maskAlpha = Sample_PreviewMaskTexture(maskUV, maskUVMode);

    const float breakupNoise = lerp(1.f, noiseRaw, noiseStrength);
    float coverage = maskAlpha;

    coverage = Effect_ApplyAlphaErosion(coverage, maskAlpha * breakupNoise, alphaErosion);

    const float energyVariation = lerp(1.f, lerp(0.75f, 1.25f, noiseRaw), noiseStrength);
    const float facing = saturate(abs(In.normal.z) * 0.35f + 0.65f);
    const float3 baseColor = Effect_ApplyCoreEmissiveShaping(
        symbol.rgb * g_Tint.rgb,
        g_EffectMaterialPreviewCoreEmissiveColor,
        selectedOpacity,
        g_EffectMaterialPreviewCoreEmissiveParams
    );
    surface.color = baseColor * energyVariation * facing;
    surface.coverage = Effect_BuildCoverage(selectedOpacity, opacityPower, coverage, breakupNoise, g_Tint.a);

    if (surface.coverage < max(alphaCutoff, 0.01f))
        discard;

    return surface;
}

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMaterialSurface(In);
    Out.color = Effect_ComposeAlphaBlend(surface, g_EffectMaterialPreviewParams.x);
    return Out;
}

PS_OUT PS_ADDITIVE(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float2 mainUV = Apply_MaterialPreviewUV(In.texCoord, g_EffectMaterialPreviewMainUVParams, g_EffectMaterialPreviewUVOffsetParams.xy);
    const float2 noiseUV = Apply_MaterialPreviewUV(In.texCoord, g_EffectMaterialPreviewNoiseUVParams, g_EffectMaterialPreviewUVOffsetParams.zw);
    const float2 maskUV = Apply_MaterialPreviewUV(In.texCoord, g_EffectMaterialPreviewMaskUVParams, g_EffectMaterialPreviewMaskUVOffsetParams.xy);
    const float mainUVMode = g_EffectMaterialPreviewUVModeParams.x;
    const float noiseUVMode = g_EffectMaterialPreviewUVModeParams.y;
    const float maskUVMode = g_EffectMaterialPreviewUVModeParams.z;
    const float4 symbol = Sample_PreviewMainTexture(mainUV, mainUVMode);
    const float alphaCutoff = saturate(g_EffectMaterialPreviewAlphaParams.x);
    const float alphaErosion = saturate(g_EffectMaterialPreviewAlphaParams.y);
    float noiseRaw = 1.f;
    float maskAlpha = 1.f;

    const float noiseStrength = saturate(g_EffectMaterialPreviewParams.z);
    if (g_EffectMaterialPreviewParams.w != 0 && g_EffectMaterialPreviewParams.z > 0.f)
        noiseRaw = Sample_PreviewNoiseTexture(noiseUV, noiseUVMode);

    if (g_EffectMaterialPreviewAlphaParams.z != 0)
        maskAlpha = Sample_PreviewMaskTexture(maskUV, maskUVMode);

    const float breakupNoise = lerp(1.f, noiseRaw, noiseStrength);
    const float coverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * breakupNoise, alphaErosion);
    const float coverageBase = saturate(coverage * breakupNoise * g_Tint.a);
    const float energyVariation = lerp(1.f, lerp(0.75f, 1.25f, noiseRaw), noiseStrength);
    const float facing = saturate(abs(In.normal.z) * 0.35f + 0.65f);
    const int colorSource = (int)g_EffectMaterialPreviewAdditiveParams.x;
    const int amountSource = (int)g_EffectMaterialPreviewAdditiveParams.y;
    const int coveragePolicy = (int)g_EffectMaterialPreviewAdditiveParams.z;
    const float selectedOpacity = Effect_SelectOpacity(symbol, g_OpacitySource);
    const float amount = Effect_SelectAdditiveAmount(symbol, maskAlpha, amountSource);
    const float policyAmount = Effect_ResolveAdditivePolicyAmount(amount, coverageBase, coveragePolicy);
    const float discardSource = Effect_ResolveAdditiveDiscardSource(amount, coverageBase, coveragePolicy);
    const float blackGate = Effect_ResolveAdditiveBlackGate(symbol.rgb, colorSource, g_EffectMaterialPreviewAdditiveFlags.x);

    if (discardSource < max(alphaCutoff, 0.01f))
        discard;

    const float3 materialColor = Effect_ResolveAdditiveColor(
        symbol.rgb,
        symbol.rgb * g_Tint.rgb,
        g_EffectMaterialPreviewAdditiveEmissiveColor,
        g_EffectMaterialPreviewAdditiveConstantColor,
        colorSource
    );
    const float3 shapedMaterialColor = Effect_ApplyCoreEmissiveShaping(
        materialColor,
        g_EffectMaterialPreviewCoreEmissiveColor,
        selectedOpacity,
        g_EffectMaterialPreviewCoreEmissiveParams
    );
    const float additiveEnergyScalar = max(g_EffectMaterialPreviewParams.x, 0.f) * max(g_EffectMaterialPreviewAdditiveParams.w, 0.f);
    const float3 contribution = shapedMaterialColor * energyVariation * facing * policyAmount * additiveEnergyScalar * blackGate;
    Out.color = float4(contribution, 1.f);
    return Out;
}

PS_OUT PS_CHECKER(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float2 targetSize = max(g_EffectMaterialPreviewCheckerParams.xy, float2(1.f, 1.f));
    const float2 aspectUV = float2(In.texCoord.x * targetSize.x / targetSize.y, In.texCoord.y);
    const float2 coarseCell = floor(aspectUV * 10.f);
    const float checker = fmod(coarseCell.x + coarseCell.y, 2.f);
    const float2 grid = abs(frac(aspectUV * 10.f) - 0.5f);
    const float gridLine = 1.f - smoothstep(0.455f, 0.495f, max(grid.x, grid.y));
    const float vignette = saturate(1.f - length(In.texCoord - 0.5f) * 0.95f);
    const float3 darkColor = float3(0.10f, 0.12f, 0.16f);
    const float3 lightColor = float3(0.34f, 0.39f, 0.46f);
    float3 color = lerp(darkColor, lightColor, checker);

    color = lerp(color, float3(0.72f, 0.78f, 0.86f), gridLine * 0.24f);
    color *= lerp(0.72f, 1.08f, vignette);

    Out.color = float4(color, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    // Plane preview
    PASS_RS_DS_BS_VP(AlphaBlendPass, RS_CullNone, DSS_Default, BS_EffectAlphaBlend, VS_MAIN, PS_MAIN) // 0

    // Sphere preview
    PASS_RS_DS_BS_VP(SphereAlphaBlendPass, RS_Default, DSS_Default, BS_EffectAlphaBlend, VS_MAIN, PS_MAIN) // 1

    // Plane additive preview
    PASS_RS_DS_BS_VP(AdditivePass, RS_CullNone, DSS_Default, BS_EffectAdditive, VS_MAIN, PS_ADDITIVE) // 2

    // Sphere additive preview
    PASS_RS_DS_BS_VP(SphereAdditivePass, RS_Default, DSS_Default, BS_EffectAdditive, VS_MAIN, PS_ADDITIVE) // 3

    // Preview-local checker source/background
    PASS_RS_DS_BS_VP(CheckerBackgroundPass, RS_CullNone, DSS_None, BS_Default, VS_CHECKER, PS_CHECKER) // 4
}
