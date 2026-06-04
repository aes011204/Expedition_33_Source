#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"

Texture2D g_CloudAlphaTexture;

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
    float3 WorldNormal : TEXCOORD1;
};

VS_OUT VS_Default(VS_IN In)
{
    VS_OUT Out;

    float4 worldPos = mul(float4(In.Position, 1.f), g_WorldMatrix);
    Out.Position = mul(mul(worldPos, g_ViewMatrix), g_ProjMatrix);
    Out.Texcoord = In.Texcoord;
    Out.WorldNormal = normalize(mul(float4(In.Normal, 0.f), g_WorldMatrix).xyz);

    return Out;
}

struct PS_OUT
{
    float4 Color : SV_TARGET0;
};

PS_OUT PS_Default(VS_OUT In)
{
    PS_OUT Out;

    const float4 tint = g_FeatureParam0;
    const float3 skyScatter = g_FeatureParam1.rgb;
    const float3 groundScatter = g_FeatureParam2.rgb;
    const float scatterBlend = g_FeatureParam3.x;
    const float shadowBlur = max(g_FeatureParam3.y, 0.0001f);
    const float glow = g_FeatureParam3.z;
    const float opacity = g_FeatureParam3.w;

    float alpha = g_CloudAlphaTexture.Sample(LinearClampSampler, In.Texcoord).r;
    float facing = saturate(In.WorldNormal.y * 0.5f + 0.5f + scatterBlend);
    float3 scatter = lerp(groundScatter, skyScatter, facing);
    float softAlpha = smoothstep(shadowBlur, 1.f, alpha);

    Out.Color = float4(tint.rgb * scatter * glow, softAlpha * opacity * tint.a);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(AlphaBlendTwoSided, RS_CullNone, DSS_DepthRead, BS_EffectAlphaBlend, VS_Default, PS_Default)
}
