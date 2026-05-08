#include "Engine_Shader_Defines.hlsli"
//==================================================
// MATRIX
//==================================================
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

cbuffer BoneTransform : register(b1)
{
    matrix g_BoneMatrices[950];
}

//==================================================
// MATERIAL
//==================================================
Texture2D g_DiffuseTexture;
Texture2D g_NormalTexture;
Texture2D g_OpacityTexture;
Texture2D g_EmissiveTexture;

//==================================================
// CAMERA
//==================================================
float g_fFar = 0.f;

//==================================================
// SHADOW
//==================================================
float g_fLightFar = 0.f;

//==================================================
// VALUE
//==================================================
float2 g_UVScale = float2(1.f, 1.f);
float2 g_UVOffset = float2(0.f, 0.f);
float  g_UVRotation = 0.f;
int    g_UseOpacityMask = 0;

//==================================================
// SAMPLER
//==================================================
SamplerState DefaultSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

//==================================================
// VS
//==================================================
struct VS_IN
{
    float3  vPosition : POSITION;
    float3  vNormal : NORMAL;
    float3  vTangent : TANGENT;
    float3  vBinormal : BINORMAL;
    float2  vTexcoord : TEXCOORD0;
    uint4   vBlendIndex : BLENDINDEX;
    float4  vBlendWeight : BLENDWEIGHT;
};

struct VS_OUT
{
    float4  vPosition : SV_POSITION;
    float4  vNormal : NORMAL;
    float3  vTangent : TANGENT;
    float3  vBinormal : BINORMAL;
    float2  vTexcoord : TEXCOORD0;
    float4  vWorldPos : TEXCOORD1;
    float4  vProjPos : TEXCOORD2;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    /* 정점의 위치 * 월드 * 뷰 * 투영 */
    float4x4 matWV, matWVP;

    float fWeightW = 1.f - (In.vBlendWeight.x + In.vBlendWeight.y + In.vBlendWeight.z);

    matrix BoneMatrix = g_BoneMatrices[In.vBlendIndex.x] * In.vBlendWeight.x +
        g_BoneMatrices[In.vBlendIndex.y] * In.vBlendWeight.y +
        g_BoneMatrices[In.vBlendIndex.z] * In.vBlendWeight.z +
        g_BoneMatrices[In.vBlendIndex.w] * fWeightW;

    vector vPosition = mul(float4(In.vPosition, 1.f), BoneMatrix);
    vector vNormal = mul(float4(In.vNormal, 0.f), BoneMatrix);
    vector vTangent = mul(float4(In.vTangent, 0.f), BoneMatrix);
    vector vBinormal = mul(float4(In.vBinormal, 0.f), BoneMatrix);

    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(vPosition, matWVP);
    Out.vNormal = normalize(mul(vNormal, g_WorldMatrix));

    Out.vTangent = normalize(mul(vTangent, g_WorldMatrix));
    Out.vBinormal = normalize(mul(vBinormal, g_WorldMatrix));

    Out.vTexcoord = In.vTexcoord;
    Out.vWorldPos = mul(vPosition, g_WorldMatrix);
    Out.vProjPos = Out.vPosition;

    return Out;
}

// UV 변환 함수 하나 추가
float2 ApplyUVTransform(float2 uv, float2 scale, float2 offset, float rotation)
{
    float2 scaled = uv * scale;

    float s = sin(rotation);
    float c = cos(rotation);

    float2 rotated;
    rotated.x = scaled.x * c - scaled.y * s;
    rotated.y = scaled.x * s + scaled.y * c;

    return rotated + offset;
}

//==================================================
// PS
//==================================================
struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float3 vTangent : TANGENT;
    float3 vBinormal : BINORMAL;
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

/* Pixel Shader -> 픽셀의 색을 결정한다. */

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.vTexcoord, g_UVScale, g_UVOffset, g_UVRotation);

    float4 baseColor = g_DiffuseTexture.Sample(DefaultSampler, finalUV);

    float opacity = baseColor.a;
    if (g_UseOpacityMask != 0)
    {
        float opacityFromMask = g_OpacityTexture.Sample(DefaultSampler, finalUV).r;
        opacity = opacityFromMask;
    }

    if (opacity < 0.3f)
        discard;

    float3 worldNormal = normalize(In.vNormal.xyz);

    Out.vDiffuse = baseColor;
    Out.vNormal = float4(worldNormal * 0.5f + 0.5f, 1.f);
    Out.vDepth = float4(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_fFar, 0.f, 1.f);

    return Out;
}


struct PS_OUT_SHADOW
{
    vector vLightDepth : SV_TARGET0;
};

PS_OUT_SHADOW PS_MAIN_SHADOW(PS_IN In)
{
    PS_OUT_SHADOW Out;

    Out.vLightDepth = vector(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_fLightFar, 0.f, 1.f);
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

    pass ShadowPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_MAIN_SHADOW();
    }
}
