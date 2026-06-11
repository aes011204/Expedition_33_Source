#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"

float4x4 g_WorldMatrix;
float4x4 g_ViewMatrix;
float4x4 g_ProjMatrix;

Texture2D g_Texture;
float4 g_TintColor = float4(1.f, 1.f, 1.f, 1.f);
float4 g_UVRect = float4(0.f, 0.f, 1.f, 1.f);

struct VS_IN
{
    float3 vPosition : POSITION;
    float2 vTexCoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexCoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP);
    Out.vTexCoord = lerp(g_UVRect.xy, g_UVRect.zw, In.vTexCoord);

    return Out;
}

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float2 vTexCoord : TEXCOORD0;
};

struct PS_OUT
{
    float4 vColor : SV_TARGET0;
};

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out;

    Out.vColor = g_Texture.Sample(LinearClampSampler, In.vTexCoord) * g_TintColor;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DepthReadPass, RS_CullNone, DSS_DepthRead, BS_AlphaBlend, VS_MAIN, PS_MAIN)
    PASS_RS_DS_BS_VP(NoDepthPass, RS_CullNone, DSS_None, BS_AlphaBlend, VS_MAIN, PS_MAIN)
}
