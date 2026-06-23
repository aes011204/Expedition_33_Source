#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"


float    g_BarRatio;

struct VS_IN
{
    float3 vPosition : POSITION;
    float2 vTexcoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV = mul(g_WorldMatrix, g_ScreenViewMatrix);
    float4x4 matWVP = mul(matWV, g_ScreenProjMatrix);

    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP);
    Out.vTexcoord = In.vTexcoord;

    return Out;
}

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
};

struct PS_OUT
{
    float4 vColor : SV_TARGET0;
};

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out;

    if (In.vTexcoord.y < g_BarRatio || In.vTexcoord.y > 1.0f - g_BarRatio)
    {
        Out.vColor = float4(0.f, 0.f, 0.f, 1.f);
        return Out;
    }

    discard;
 
    Out.vColor = float4(0.f, 0.f, 0.f, 0.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(LetterboxPass, RS_Default, DSS_None, BS_AlphaBlend, VS_MAIN, PS_MAIN)
}
