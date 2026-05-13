#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Material.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Utils.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"

cbuffer BoneTransform : register(b1)
{
    matrix g_BoneMatrices[950];
}

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

VS_OUT VS_Main(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV, matWVP;

    float fWeightW = 1.f - (In.vBlendWeight.x + In.vBlendWeight.y + In.vBlendWeight.z);

    float4x4 BoneMatrix = g_BoneMatrices[In.vBlendIndex.x] * In.vBlendWeight.x +
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

VS_OUT VS_Shadow(VS_IN In)
{
    VS_OUT Out;

    float fWeightW = 1.f - (In.vBlendWeight.x + In.vBlendWeight.y + In.vBlendWeight.z);

    float4x4 BoneMatrix = g_BoneMatrices[In.vBlendIndex.x] * In.vBlendWeight.x +
        g_BoneMatrices[In.vBlendIndex.y] * In.vBlendWeight.y +
        g_BoneMatrices[In.vBlendIndex.z] * In.vBlendWeight.z +
        g_BoneMatrices[In.vBlendIndex.w] * fWeightW;

    vector vPosition = mul(float4(In.vPosition, 1.f), BoneMatrix);
    vector vNormal = mul(float4(In.vNormal, 0.f), BoneMatrix);
    vector vTangent = mul(float4(In.vTangent, 0.f), BoneMatrix);
    vector vBinormal = mul(float4(In.vBinormal, 0.f), BoneMatrix);

    float4x4 matWV = mul(g_WorldMatrix, g_LightViewMatrix);
    float4x4 matWVP = mul(matWV, g_LightProjMatrix);

    Out.vPosition = mul(vPosition, matWVP);
    Out.vNormal = normalize(mul(vNormal, g_WorldMatrix));
    Out.vTangent = normalize(mul(vTangent, g_WorldMatrix));
    Out.vBinormal = normalize(mul(vBinormal, g_WorldMatrix));
    Out.vTexcoord = In.vTexcoord;
    Out.vWorldPos = mul(vPosition, g_WorldMatrix);
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

PS_OUT PS_Opaque(PS_IN In)
{
    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.vTexcoord);

    float4 baseColor = SampleBaseColor(finalUV);

    float3 worldNormal = ComputeWorldNormal(In.vNormal.xyz, In.vTangent, In.vBinormal, finalUV);

    float3 orm = SampleOrm(finalUV);

    // emissive 는 이후에 별도 RTV로 분리해야 한다. 일단은 텍스처 잘 들어오는지 확
    float3 emissive = SampleEmissive(finalUV);
    baseColor.rgb = baseColor.rgb * orm.r + emissive;

    Out.vDiffuse = baseColor;
    Out.vNormal = float4(EncodeNormal(worldNormal), 1.f);
    Out.vDepth = float4(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_Far, 0.f, 1.f);

    return Out;
}

PS_OUT PS_Mask(PS_IN In)
{
    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.vTexcoord);

    float4 baseColor = SampleBaseColor(finalUV);

    float mask = GetOpacityMaskForMasked(finalUV, baseColor);
    if (mask < g_OpacityMaskClipValue)
        discard;

    float3 worldNormal = ComputeWorldNormal(In.vNormal.xyz, In.vTangent, In.vBinormal, finalUV);

    float3 orm = SampleOrm(finalUV);
    float3 emissive = SampleEmissive(finalUV);
    baseColor.rgb = baseColor.rgb * orm.r + emissive;

    Out.vDiffuse = baseColor;
    Out.vNormal = float4(EncodeNormal(worldNormal), 1.f);
    Out.vDepth = float4(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_Far, 0.f, 1.f);

    return Out;
}

struct PS_OUT_SHADOW
{
    vector vLightDepth : SV_TARGET0;
};

PS_OUT_SHADOW PS_Shadow(PS_IN In)
{
    PS_OUT_SHADOW Out;

    Out.vLightDepth = vector(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_LightFar, 0.f, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(OpaquePass,         RS_Default,  DSS_Default, BS_Default, VS_Main,     PS_Opaque)      // 0
    PASS_RS_DS_BS_VP(MaskedPass,         RS_Default,  DSS_Default, BS_Default, VS_Main,     PS_Mask)        // 1
    PASS_RS_DS_BS_VP(OpaqueTwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_Main,     PS_Opaque)      // 2
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_Main,     PS_Mask)        // 3
    PASS_RS_DS_BS_VP(ShadowPass,         RS_Default,  DSS_Default, BS_Default, VS_Shadow,   PS_Shadow)      // 4
}   
