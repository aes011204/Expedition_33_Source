#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"

Texture2D g_SkyTexture;
Texture2D g_FlowTexture;
Texture2D g_NoiseTexture;

struct VS_IN
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float2 Texcoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float2 Texcoord : TEXCOORD0;
};

VS_OUT VS_Default(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.Position = mul(float4(In.Position, 1.f), matWVP);
    Out.Texcoord = In.Texcoord;

    return Out;
}

struct PS_OUT
{
    vector Color : SV_TARGET0;
};

PS_OUT PS_Default(VS_OUT In)
{
    PS_OUT Out;

    const float4 tint = g_FeatureParam0;
    const float brightness = g_FeatureParam1.x;
    const float contrast = g_FeatureParam1.y;
    const float noiseStrength = g_FeatureParam1.z;
    const float flowOffset = g_FeatureParam1.w;

    float2 flowUV = In.Texcoord + flowOffset;
    float2 flowSample = g_FlowTexture.Sample(LinearWrapSampler, flowUV).rg * 2.f - 1.f;

    // 원본 M_Flowmap_CleasTower처럼 T_flowmap_03을 FlowTexture와 NoiseTexture 양쪽에 쓴다.
    float noise = g_NoiseTexture.Sample(LinearWrapSampler, flowUV).r;
    float2 skyUV = In.Texcoord + flowSample * noise * noiseStrength;

    float3 color = g_SkyTexture.Sample(LinearWrapSampler, skyUV).rgb;
    color = (color - 0.5f) * contrast + 0.5f;
    color *= tint.rgb;
    color *= brightness / 500.f;

    Out.Color = float4(saturate(color), 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DefaultPass, RS_CullNone, DSS_None, BS_Default, VS_Default, PS_Default)
}
