#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
uint g_iObjectID;

float g_fCameraFar;
Texture2D g_DiffuseTexture;

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
};

struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float3 vTangent : TANGENT;
    float2 vTexcoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;
    float4x4 matWVP = mul(mul(g_WorldMatrix, g_ViewMatrix), g_ProjMatrix);
    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP);
    Out.vNormal = normalize(mul(float4(In.vNormal, 0.f), g_WorldMatrix));
    Out.vTexcoord = In.vTexcoord;
    return Out;
}

struct PS_OUT_GBUFFER
{
    vector vDiffuse : SV_Target0;
    vector vNormal : SV_Target1;
    vector vDepth : SV_Target2;
};

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
};

struct PS_OUT
{
    vector vColor : SV_TARGET0;
};


PS_OUT_GBUFFER PS_MAIN(PS_IN In)
{
    PS_OUT_GBUFFER Out;

    vector vMtrlDiffuse = g_DiffuseTexture.Sample(LinearWrapSampler, In.vTexcoord);
    if (vMtrlDiffuse.a < 0.3f)
        discard;

    Out.vDiffuse = vMtrlDiffuse;
    Out.vNormal = vector(In.vNormal.xyz * 0.5f + 0.5f, 1.f);
    Out.vDepth = vector(In.vPosition.z, In.vPosition.w / g_fCameraFar, 0.f, 0.f);
    return Out;
}

PS_OUT_GBUFFER PS_MAIN_NOTEXTURE(PS_IN In)
{
    PS_OUT_GBUFFER Out;
    Out.vDiffuse = vector(0.8f, 0.8f, 0.8f, 1.f);
    Out.vNormal = vector(In.vNormal.xyz * 0.5f + 0.5f, 1.f);
    Out.vDepth = vector(In.vPosition.z, In.vPosition.w / g_fCameraFar, 0.f, 0.f);
    return Out;
}

PS_OUT PS_MAIN_ID(PS_IN In)
{
    PS_OUT Out;
    Out.vColor.r = float(g_iObjectID & 0xFF) / 255.f;
    Out.vColor.g = float((g_iObjectID >> 8) & 0xFF) / 255.f;
    Out.vColor.b = float((g_iObjectID >> 16) & 0xFF) / 255.f;
    Out.vColor.a = 1.f;
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DefaultPass,   RS_Default, DSS_Default,    BS_Default, VS_MAIN, PS_MAIN)
    PASS_RS_DS_BS_VP(NoTexturePass, RS_Default, DSS_Default,    BS_Default, VS_MAIN, PS_MAIN_NOTEXTURE)
    PASS_RS_DS_BS_VP(IDPass,        RS_Default, DSS_Default,    BS_Default, VS_MAIN, PS_MAIN_ID)
}
