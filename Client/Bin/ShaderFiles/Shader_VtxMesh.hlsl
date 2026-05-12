#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Material.hlsl"
#include "Engine_Shader_Utils.hlsl"

//==================================================
// MATRIX
//==================================================
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
float g_fFar = 0.f;
float g_fLightFar = 0.f;
uint g_iObjectID;

//==================================================
// VS
//==================================================
struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float3 vTangent : TANGENT;
    float3 vBinormal : BINORMAL;
    float2 vTexcoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float3 vTangent : TANGENT;
    float3 vBinormal : BINORMAL;
    float2 vTexcoord : TEXCOORD0;
    float4 vWorldPos : TEXCOORD1;
    float4 vProjPos : TEXCOORD2;
};

VS_OUT VS_Default(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV, matWVP;

    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP);
    Out.vNormal = float4(normalize(mul(float4(In.vNormal, 0.f), g_WorldMatrix)).xyz, 0.f);

    Out.vTangent = normalize(mul(float4(In.vTangent, 0.f), g_WorldMatrix)).xyz;
    Out.vBinormal = normalize(mul(float4(In.vBinormal, 0.f), g_WorldMatrix)).xyz;

    Out.vTexcoord = In.vTexcoord;
    Out.vWorldPos = mul(float4(In.vPosition, 1.f), g_WorldMatrix);
    Out.vProjPos = Out.vPosition;

    return Out;
}

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

struct PS_OUT_ID
{
    vector vColor : SV_TARGET0;
};

PS_OUT PS_Opaque(PS_IN In)
{
    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.vTexcoord);

    float4 baseColor = SampleBaseColor(finalUV);

    float3 worldNormal = ComputeWorldNormal(In.vNormal.xyz, In.vTangent, In.vBinormal, finalUV);

    float3 orm = SampleOrm(finalUV);

    // emissive 는 이후에 별도 RTV로 분리해야 한다. 일단은 텍스처 잘 들어오는지 확인만
    float3 emissive = SampleEmissive(finalUV);
    baseColor.rgb = baseColor.rgb * orm.r + emissive;

    Out.vDiffuse = baseColor;
    Out.vNormal = float4(EncodeNormal(worldNormal), 1.f);
    Out.vDepth = float4(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_fFar, 0.f, 1.f);

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

    // emissive 는 이후에 별도 RTV로 분리해야 한다. 일단은 텍스처 잘 들어오는지 확인만
    float3 emissive = SampleEmissive(finalUV);
    baseColor.rgb = baseColor.rgb * orm.r + emissive;

    Out.vDiffuse = baseColor;
    Out.vNormal = float4(EncodeNormal(worldNormal), 1.f);
    Out.vDepth = float4(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_fFar, 0.f, 1.f);

    return Out;
}

struct PS_OUT_SHADOW
{
    vector vLightDepth : SV_TARGET0;
};

PS_OUT_SHADOW PS_Shadow(PS_IN In)
{
    PS_OUT_SHADOW Out;

    Out.vLightDepth = float4(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_fLightFar, 0.f, 1.f);

    return Out;
}

PS_OUT PS_NoTexture(PS_IN In)
{
    PS_OUT Out;

    Out.vDiffuse = float4(0.8f, 0.8f, 0.8f, 1.f);
    Out.vNormal = vector(In.vNormal.xyz * 0.5f + 0.5f, 1.f);
    Out.vDepth = vector(In.vPosition.z, In.vPosition.w / g_fFar, 0.f, 0.f);

    return Out;
}

PS_OUT_ID PS_ID(PS_IN In)
{
    PS_OUT_ID Out;

    Out.vColor.r = float(g_iObjectID & 0xFF) / 255.f;
    Out.vColor.g = float((g_iObjectID >> 8) & 0xFF) / 255.f;
    Out.vColor.b = float((g_iObjectID >> 16) & 0xFF) / 255.f;
    Out.vColor.a = 1.f;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DefaultPass,         RS_Default,  DSS_Default, BS_Default, VS_Default, PS_Opaque)      // 0
    PASS_RS_DS_BS_VP(ShadowPass,          RS_Default,  DSS_Default, BS_Default, VS_Default, PS_Shadow)      // 1
    PASS_RS_DS_BS_VP(NoTexturePass,       RS_Default,  DSS_Default, BS_Default, VS_Default, PS_NoTexture)   // 2
    PASS_RS_DS_BS_VP(MaskedPass,          RS_Default,  DSS_Default, BS_Default, VS_Default, PS_Mask)        // 3
    PASS_RS_DS_BS_VP(TwoSidedPass,        RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_Opaque)      // 4
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass,  RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_Mask)        // 5
    PASS_RS_DS_BS_VP(IDPass,              RS_Default,  DSS_Default, BS_Default, VS_Default, PS_ID)          // 6
}
