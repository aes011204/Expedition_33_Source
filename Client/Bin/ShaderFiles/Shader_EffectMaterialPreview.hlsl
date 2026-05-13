#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"

float4x4 g_WorldMatrix;
float4x4 g_ViewMatrix;
float4x4 g_ProjMatrix;

cbuffer cbEffectMaterialPreview : register(b2)
{
    float4 g_vTint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_vEffectMaterialPreviewParams = { 1.f, 1.f, 0.f, 0.f }; // x: intensity, y: opacityPower, z: noiseStrength, w: useNoise
    int g_iOpacitySource = 0;
    float3 g_vEffectMaterialPreviewPadding;
};

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);

struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float3 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    const float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    const float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP);
    Out.vNormal = normalize(mul(float4(In.vNormal, 0.f), g_WorldMatrix).xyz);
    Out.vTexcoord = In.vTexcoord;

    return Out;
}

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float3 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
};

struct PS_OUT
{
    float4 vColor : SV_TARGET0;
};

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float4 vSymbol = g_Texture.Sample(LinearClampSampler, In.vTexcoord);
    const float fOpacityPower = max(g_vEffectMaterialPreviewParams.y, 0.0001f);
    float fSelectedOpacity = vSymbol.a;
    float fNoiseFactor = 1.f;

    if (1 == g_iOpacitySource)
        fSelectedOpacity = vSymbol.r;
    else if (2 == g_iOpacitySource)
        fSelectedOpacity = dot(vSymbol.rgb, float3(0.299f, 0.587f, 0.114f));

    if (0 != g_vEffectMaterialPreviewParams.w && g_vEffectMaterialPreviewParams.z > 0.f)
    {
        const float fNoise = g_NoiseTexture.Sample(LinearClampSampler, In.vTexcoord).r;
        fNoiseFactor = lerp(1.f, fNoise, saturate(g_vEffectMaterialPreviewParams.z));
    }

    const float fOpacity = pow(saturate(fSelectedOpacity), fOpacityPower);
    const float fFacing = saturate(abs(In.vNormal.z) * 0.35f + 0.65f);
    Out.vColor.rgb = vSymbol.rgb * g_vTint.rgb * g_vEffectMaterialPreviewParams.x * fNoiseFactor * fFacing;
    Out.vColor.a = fOpacity * g_vTint.a;

    if (Out.vColor.a < 0.01f)
        discard;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(AlphaBlendPass, RS_CullNone, DSS_Default, BS_AlphaBlend, VS_MAIN, PS_MAIN) // 0
    PASS_RS_DS_BS_VP(AdditivePass, RS_CullNone, DSS_Default, BS_Blend, VS_MAIN, PS_MAIN) // 1
}
