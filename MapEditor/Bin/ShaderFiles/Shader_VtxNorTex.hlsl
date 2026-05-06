#include "Engine_Shader_Defines.hlsli"

//==================================================
// MATRIX
//==================================================
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

//==================================================
// CAMERA
//==================================================
float g_fFar = 0.f;

//==================================================
// TEXTURE
//==================================================
texture2D g_DiffuseTexture;

vector g_vBrushPosition = vector(70.f, 0.f, 20.f, 1.f);

//==================================================
// SAMPLER
//==================================================
sampler DefaultSampler = sampler_state
{
    Filter = MIN_MAG_MIP_LINEAR;   
    AddressU = wrap;
    AddressV = wrap;
};

//==================================================
// VS
//==================================================
struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
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

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;
    
    /* 정점의 위치 * 월드 * 뷰 * 투영 */
    float4x4 matWV, matWVP;
    
    /* mul : 행렬끼리의 곱하기연산을 수행한다. */ 
    /* XMVector3TransformCoord : 벡터와 행렬의 곱하기연산을 수행하고 w나눈다.*/
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

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out;
    vector vMtrlDiffuse = g_DiffuseTexture.Sample(DefaultSampler, In.vTexcoord * 30.f);
    Out.vDiffuse = vector(vMtrlDiffuse.rgb, 1.f);
    Out.vNormal = vector(In.vNormal.xyz * 0.5f + 0.5f, 1.f);
    Out.vDepth = vector(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_fFar, 0.f, 1.f);
    
    return Out;
}

technique11 DefaultTechnique
{
    pass DefaultPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_MAIN();
    }
}
