#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"

float4x4 g_PreTransformMatrix;

cbuffer cbEffectMeshMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshParams = { 1.f, 1.f, 0.f, 0.f };       // x: intensity, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectMeshAlphaParams = { 0.f, 0.f, 0.f, 0.f };  // x: alphaCutoff, y: alphaErosion, z: useMask, w: reserved
    float4 g_EffectMeshMainUVParams = { 1.f, 1.f, 0.f, 0.f }; // xy: scale, zw: scroll speed
    float4 g_EffectMeshNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectMeshMaskUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectMeshUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshUVModeParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshSourceParams = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshAdditiveParams = { 0.f, 0.f, 0.f, 1.f };
    float4 g_EffectMeshAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshAdditiveFlags = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshCoreEmissiveParams = { 0.f, 4.f, 2.f, 1.f };
    float4 g_EffectMeshCoreEmissiveColor = { 1.f, 0.85f, 0.45f, 1.f };
    int g_OpacitySource = 0;
    float g_EffectMeshMaterialTime = 0.f;
    float2 g_EffectMeshPadding;
};

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);

bool Use_EffectMeshClampSampler(float mode)
{
    return mode >= 0.5f && mode < 2.5f;
}

float2 Build_EffectMeshUV(float2 baseUV, float4 uvParams, float2 uvOffset, float mode)
{
    const float2 transformedUV = baseUV * uvParams.xy + uvOffset + uvParams.zw * g_EffectMeshMaterialTime;
    return mode >= 2.5f ? baseUV : transformedUV;
}

float4 Sample_EffectMeshMainTexture(float2 uv, float mode)
{
    return Use_EffectMeshClampSampler(mode)
           ? g_Texture.Sample(LinearClampSampler, uv)
           : g_Texture.Sample(LinearWrapSampler, uv);
}

float4 Sample_EffectMeshNoiseTexture(float2 uv, float mode)
{
    return Use_EffectMeshClampSampler(mode)
           ? g_NoiseTexture.Sample(LinearClampSampler, uv)
           : g_NoiseTexture.Sample(LinearWrapSampler, uv);
}

float4 Sample_EffectMeshMaskTexture(float2 uv, float mode)
{
    return Use_EffectMeshClampSampler(mode)
           ? g_MaskTexture.Sample(LinearClampSampler, uv)
           : g_MaskTexture.Sample(LinearWrapSampler, uv);
}

struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float3 vTangent : TANGENT;
    float3 vBinormal : BINORMAL;
    float2 vTexcoord : TEXCOORD;
    float4 vWorld0 : INST_WORLD0;
    float4 vWorld1 : INST_WORLD1;
    float4 vWorld2 : INST_WORLD2;
    float4 vWorld3 : INST_WORLD3;
    float4 vInstanceColor : INST_COLOR0;
    float2 vLifeTime : INST_LIFE0;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
    float3 vNormal : TEXCOORD1;
    float4 vInstanceColor : COLOR0;
    float2 vLifeTime : TEXCOORD2;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;
    const float4x4 mWorld = float4x4(In.vWorld0, In.vWorld1, In.vWorld2, In.vWorld3);
    const float4x4 mPreWorld = mul(g_PreTransformMatrix, mWorld);

    Out.vPosition = mul(float4(In.vPosition, 1.f), mul(mPreWorld, mul(g_ViewMatrix, g_ProjMatrix)));
    Out.vTexcoord = In.vTexcoord;
    Out.vNormal = normalize(mul(float4(In.vNormal, 0.f), mPreWorld)).xyz;
    Out.vInstanceColor = In.vInstanceColor;
    Out.vLifeTime = In.vLifeTime;
    return Out;
}

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

EffectMaterialSurface Resolve_EffectMeshSurface(VS_OUT In)
{
    EffectMaterialSurface surface = (EffectMaterialSurface)0;

    const float2 mainUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMainUVParams, g_EffectMeshUVOffsetParams.xy, g_EffectMeshUVModeParams.x);
    const float2 noiseUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshNoiseUVParams, g_EffectMeshUVOffsetParams.zw, g_EffectMeshUVModeParams.y);
    const float2 maskUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMaskUVParams, g_EffectMeshMaskUVOffsetParams.xy, g_EffectMeshUVModeParams.z);
    const float4 symbol = Sample_EffectMeshMainTexture(mainUV, g_EffectMeshUVModeParams.x);
    const float opacityPower = max(g_EffectMeshParams.y, 0.0001f);
    const float alphaCutoff = saturate(g_EffectMeshAlphaParams.x);
    const float alphaErosion = saturate(g_EffectMeshAlphaParams.y);
    const float selectedOpacity = Effect_SelectOpacity(symbol, g_OpacitySource);
    float noiseFactor = 1.f;
    float maskAlpha = 1.f;

    if (0 != g_EffectMeshParams.w && g_EffectMeshParams.z > 0.f)
    {
        const float4 noiseTexel = Sample_EffectMeshNoiseTexture(noiseUV, g_EffectMeshUVModeParams.y);
        const float noise = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectMeshSourceParams.x),
            g_EffectMeshSourceParams.z
        );
        noiseFactor = lerp(1.f, noise, saturate(g_EffectMeshParams.z));
    }

    if (0 != g_EffectMeshAlphaParams.z)
    {
        const float4 maskTexel = Sample_EffectMeshMaskTexture(maskUV, g_EffectMeshUVModeParams.z);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectMeshSourceParams.y),
            g_EffectMeshSourceParams.w
        );
    }

    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * noiseFactor, alphaErosion);
    const float coverage = Effect_BuildCoverage(selectedOpacity, opacityPower, maskCoverage, noiseFactor, In.vInstanceColor.a * g_Tint.a);
    const float3 baseColor = Effect_ApplyCoreEmissiveShaping(
        symbol.rgb * In.vInstanceColor.rgb * g_Tint.rgb,
        g_EffectMeshCoreEmissiveColor,
        selectedOpacity,
        g_EffectMeshCoreEmissiveParams
    );

    surface.color = baseColor * noiseFactor;
    surface.coverage = coverage;

    if (coverage < max(alphaCutoff, 0.01f))
        discard;

    return surface;
}

PS_OUT PS_ALPHA(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMeshSurface(In);
    Out.color = Effect_ComposeAlphaBlend(surface, g_EffectMeshParams.x);
    return Out;
}

PS_OUT PS_MASKED(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMeshSurface(In);
    Out.color = float4(surface.color * max(g_EffectMeshParams.x, 0.f), 1.f);
    return Out;
}

PS_OUT PS_ADDITIVE(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;

    const float2 mainUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMainUVParams, g_EffectMeshUVOffsetParams.xy, g_EffectMeshUVModeParams.x);
    const float2 noiseUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshNoiseUVParams, g_EffectMeshUVOffsetParams.zw, g_EffectMeshUVModeParams.y);
    const float2 maskUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMaskUVParams, g_EffectMeshMaskUVOffsetParams.xy, g_EffectMeshUVModeParams.z);
    const float4 symbol = Sample_EffectMeshMainTexture(mainUV, g_EffectMeshUVModeParams.x);
    const float alphaCutoff = saturate(g_EffectMeshAlphaParams.x);
    const float alphaErosion = saturate(g_EffectMeshAlphaParams.y);
    float noiseFactor = 1.f;
    float maskAlpha = 1.f;

    if (0 != g_EffectMeshParams.w && g_EffectMeshParams.z > 0.f)
    {
        const float4 noiseTexel = Sample_EffectMeshNoiseTexture(noiseUV, g_EffectMeshUVModeParams.y);
        const float noise = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectMeshSourceParams.x),
            g_EffectMeshSourceParams.z
        );
        noiseFactor = lerp(1.f, noise, saturate(g_EffectMeshParams.z));
    }

    if (0 != g_EffectMeshAlphaParams.z)
    {
        const float4 maskTexel = Sample_EffectMeshMaskTexture(maskUV, g_EffectMeshUVModeParams.z);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectMeshSourceParams.y),
            g_EffectMeshSourceParams.w
        );
    }

    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * noiseFactor, alphaErosion);
    const float coverageBase = saturate(maskCoverage * noiseFactor * In.vInstanceColor.a * g_Tint.a);
    const int colorSource = (int)g_EffectMeshAdditiveParams.x;
    const int amountSource = (int)g_EffectMeshAdditiveParams.y;
    const int coveragePolicy = (int)g_EffectMeshAdditiveParams.z;
    const float selectedOpacity = Effect_SelectOpacity(symbol, g_OpacitySource);
    const float amount = Effect_SelectAdditiveAmount(symbol, maskAlpha, amountSource);
    const float policyAmount = Effect_ResolveAdditivePolicyAmount(amount, coverageBase, coveragePolicy);
    const float discardSource = Effect_ResolveAdditiveDiscardSource(amount, coverageBase, coveragePolicy);
    const float blackGate = Effect_ResolveAdditiveBlackGate(symbol.rgb, colorSource, g_EffectMeshAdditiveFlags.x);

    if (discardSource < max(alphaCutoff, 0.01f))
        discard;

    const float3 materialColor = Effect_ResolveAdditiveColor(
        symbol.rgb,
        symbol.rgb * g_Tint.rgb,
        g_EffectMeshAdditiveEmissiveColor,
        g_EffectMeshAdditiveConstantColor,
        colorSource
    );
    const float3 shapedMaterialColor = Effect_ApplyCoreEmissiveShaping(
        materialColor * In.vInstanceColor.rgb,
        g_EffectMeshCoreEmissiveColor,
        selectedOpacity,
        g_EffectMeshCoreEmissiveParams
    );
    const float additiveEnergyScalar = max(g_EffectMeshParams.x, 0.f) * max(g_EffectMeshAdditiveParams.w, 0.f);
    Out.color = float4(shapedMaterialColor * noiseFactor * policyAmount * additiveEnergyScalar * blackGate * coverageBase, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(AlphaBlendPass, RS_Default, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, PS_ALPHA)          // 0
    PASS_RS_DS_BS_VP(AdditivePass, RS_Default, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, PS_ADDITIVE)           // 1
    PASS_RS_DS_BS_VP(AlphaBlendTwoSidedPass, RS_CullNone, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, PS_ALPHA) // 2
    PASS_RS_DS_BS_VP(AdditiveTwoSidedPass, RS_CullNone, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, PS_ADDITIVE)  // 3
    PASS_RS_DS_BS_VP(MaskedPass, RS_Default, DSS_Default, BS_Default, VS_MAIN, PS_MASKED)                        // 4
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_MAIN, PS_MASKED)               // 5
}
