#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"

//==================================================
// MATRIX
//==================================================
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

//==================================================
// MATERIAL
//==================================================
Texture2D g_DiffuseTexture;

//==================================================
// CAMERA
//==================================================
float g_fFar = 0.f;

//==================================================
// SHADOW
//==================================================
float g_fLightFar = 0.f;

//==================================================
// VS
//==================================================
struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float3 vTangent : TANGENT;
    float2 vTexcoord : TEXCOORD0;    
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
    float4 vWorldPos : TEXCOORD1;
    float4 vProjPos : TEXCOORD2;
};

VS_OUT VS_Default(VS_IN In)
{
    VS_OUT Out;
    
    /* 정점의 위치 * 월드 * 뷰 * 투영 */
    float4x4 matWV, matWVP;
    
    /* mul : 행렬끼리의 곱하기연산을 수행한다. */ 
    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);
    
    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP); 
    Out.vNormal = normalize(mul(float4(In.vNormal, 0.f), g_WorldMatrix));
    
    Out.vTexcoord = In.vTexcoord;
    Out.vWorldPos = mul(float4(In.vPosition, 1.f), g_WorldMatrix);
    Out.vProjPos = Out.vPosition;

    return Out;
}

//==================================================
// PS
//==================================================
struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;    
    float4 vWorldPos : TEXCOORD1;
    float4 vProjPos : TEXCOORD2;    
};

struct PS_OUT
{
    vector vDiffuse : SV_TARGET0;
    vector vNormal : SV_TARGET1;
    vector vDepth : SV_TARGET2;
};

PS_OUT PS_Default(PS_IN In)
{
    PS_OUT Out;
    
    vector      vMtrlDiffuse = g_DiffuseTexture.Sample(LinearWrapSampler, In.vTexcoord);
    
    if (vMtrlDiffuse.a < 0.3f)
        discard;

    Out.vDiffuse = vMtrlDiffuse;

    Out.vNormal = vector(In.vNormal.xyz * 0.5f + 0.5f, 1.f);    
    
    Out.vDepth = vector(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_fFar, 0.f, 1.f);

    return Out;

}

struct PS_OUT_SHADOW
{
    vector vLightDepth : SV_TARGET0;
};

PS_OUT_SHADOW PS_Shadow(PS_IN In)
{
    PS_OUT_SHADOW Out;
    
    Out.vLightDepth = vector(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_fLightFar, 0.f, 1.f);
    
    return Out;
}

PS_OUT PS_NoTexture(PS_IN In)
{
    PS_OUT Out;

    Out.vDiffuse = vector(0.8f, 0.8f, 0.8f, 1.f);
    Out.vNormal = vector(In.vNormal.xyz * 0.5f + 0.5f, 1.f);
    Out.vDepth = vector(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_fFar, 0.f, 1.f);

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DefaultPass,   RS_Default, DSS_Default, BS_Default, VS_Default, PS_Default)    // 0
    PASS_RS_DS_BS_VP(ShadowPass,    RS_Default, DSS_Default, BS_Default, VS_Default, PS_Shadow)     // 1
    PASS_RS_DS_BS_VP(NoTexturePass, RS_Default, DSS_Default, BS_Default, VS_Default, PS_NoTexture)  // 2
}
