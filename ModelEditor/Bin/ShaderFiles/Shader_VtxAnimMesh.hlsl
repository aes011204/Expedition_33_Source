#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"


vector g_vLightDir;
vector g_vLightDiffuse;
vector g_vLightAmbient;
vector g_vLightSpecular;

Texture2D g_DiffuseTexture;
vector g_vMtrlAmbient = vector(0.3f, 0.3f, 0.3f, 1.f);
vector g_vMtrlSpecular = vector(1.f, 1.f, 1.f, 1.f);

struct BoneMatrixElement
{
    row_major float4x4 BoneMatrix;
};
StructuredBuffer<BoneMatrixElement> g_BoneMatrices;

StructuredBuffer<BoneMatrixElement> g_BoneMatricesBuffer;

struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float3 vTangent : TANGENT;
    float3 vBinormal : BINORMAL;
    float2 vTexcoord : TEXCOORD0;
    uint4 vBlendIndex : BLENDINDEX;
    float4 vBlendWeight : BLENDWEIGHT;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
    float4 vWorldPos : TEXCOORD1;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV, matWVP;

    float fWeightW = 1.f - (In.vBlendWeight.x + In.vBlendWeight.y + In.vBlendWeight.z);

    float4x4 BoneMatrix =
        g_BoneMatricesBuffer[In.vBlendIndex.x].BoneMatrix * In.vBlendWeight.x +
        g_BoneMatricesBuffer[In.vBlendIndex.y].BoneMatrix * In.vBlendWeight.y +
        g_BoneMatricesBuffer[In.vBlendIndex.z].BoneMatrix * In.vBlendWeight.z +
        g_BoneMatricesBuffer[In.vBlendIndex.w].BoneMatrix * fWeightW;

    vector vPosition = mul(float4(In.vPosition, 1.f), BoneMatrix);
    vector vNormal = mul(float4(In.vNormal, 0.f), BoneMatrix);

    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(vPosition, matWVP);
    Out.vNormal = normalize(mul(vNormal, g_WorldMatrix));
    Out.vTexcoord = In.vTexcoord;
    Out.vWorldPos = mul(vPosition, g_WorldMatrix);

    return Out;
}

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
    float4 vWorldPos : TEXCOORD1;
};

struct PS_OUT
{
    vector vColor : SV_TARGET0;
};

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out;

    vector vMtrlDiffuse = g_DiffuseTexture.Sample(LinearWrapSampler, In.vTexcoord);

    if (vMtrlDiffuse.a < 0.3f)
        discard;

    vector vShade = saturate(max(dot(normalize(g_vLightDir) * -1.f, In.vNormal), 0.f) + (g_vLightAmbient * g_vMtrlAmbient));

    vector vLook = In.vWorldPos - g_CamPosition;
    vector vRelfect = reflect(normalize(g_vLightDir), In.vNormal);
    float fSpecular = pow(max(dot(normalize(vLook) * -1.f, normalize(vRelfect)), 0.f), 50.f);

    vector vSpecularColor = g_vLightSpecular * g_vMtrlSpecular * fSpecular;

    Out.vColor = g_vLightDiffuse * vMtrlDiffuse * vShade + vSpecularColor;
    Out.vColor.a = 1.f;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DefaultPass, RS_CullNone, DSS_Default, BS_Default, VS_MAIN, PS_MAIN)
}
