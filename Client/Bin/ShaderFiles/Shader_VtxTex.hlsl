#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
Texture2D g_Texture;

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

VS_OUT VS_Default(VS_IN In)
{
    VS_OUT Out;
    
    float4x4 matWV, matWVP;

    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);
    
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
    vector vColor : SV_TARGET0;
};

PS_OUT PS_Default(PS_IN In)
{
    PS_OUT Out;
    
    Out.vColor = g_Texture.Sample(LinearSampler, In.vTexcoord);
    Out.vColor.gb = Out.vColor.r;
    
    return Out;

}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DefaultPass, RS_Default, DSS_Default, BS_Default, VS_Default, PS_Default) // 0
}
