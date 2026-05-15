#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"

//==================================================
// MATRIX
//==================================================
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

//==================================================
// TEXTURE
//==================================================
TextureCube g_Texture;

//==================================================
// VS
//==================================================
struct VS_IN
{
    float3 Position : POSITION;
    float3 Texcoord : TEXCOORD0;    
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float3 Texcoord : TEXCOORD0;
};

VS_OUT VS_Default(VS_IN In)
{
    VS_OUT Out;
    
    float4x4 matWV, matWVP;

    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);
    
    Out.Position = mul(float4(In.Position, 1.f), matWVP); 
    Out.Texcoord = In.Texcoord;
    
    return Out;
}

//==================================================
// PS
//==================================================
struct PS_IN
{
    float4 Position : SV_POSITION;
    float3 Texcoord : TEXCOORD0;    
};

struct PS_OUT
{
    vector Color : SV_TARGET0;
};

PS_OUT PS_Default(PS_IN In)
{
    PS_OUT Out;
  
    Out.Color = g_Texture.Sample(LinearSampler, In.Texcoord);
    Out.Color.a = 1.f;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DefaultPass, RS_CullNone, DSS_None, BS_Default, VS_Default, PS_Default)
}
