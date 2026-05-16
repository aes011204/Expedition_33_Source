#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Material.hlsl"
#include "Engine_Shader_Utils.hlsl"

uint g_iObjectID;

//==================================================
// VS
//==================================================
struct VS_IN
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float3 Tangent  : TANGENT;
    float3 Binormal : BINORMAL;
    float2 Texcoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float4 Normal   : NORMAL;
    float3 Tangent  : TANGENT;
    float3 Binormal : BINORMAL;
    float2 Texcoord : TEXCOORD0;
    float4 WorldPos : TEXCOORD1;
    float4 ProjPos  : TEXCOORD2;
};

VS_OUT VS_Default(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV, matWVP;

    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);

    Out.Position = mul(float4(In.Position, 1.f), matWVP);
    Out.Normal = float4(normalize(mul(float4(In.Normal, 0.f), g_WorldMatrix)).xyz, 0.f);

    Out.Tangent = normalize(mul(float4(In.Tangent, 0.f), g_WorldMatrix)).xyz;
    Out.Binormal = normalize(mul(float4(In.Binormal, 0.f), g_WorldMatrix)).xyz;

    Out.Texcoord = In.Texcoord;
    Out.WorldPos = mul(float4(In.Position, 1.f), g_WorldMatrix);
    Out.ProjPos = Out.Position;

    return Out;
}

VS_OUT VS_Shadow(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV, matWVP;

    matWV = mul(g_WorldMatrix, g_LightViewMatrix);
    matWVP = mul(matWV, g_LightProjMatrix);

    Out.Position = mul(float4(In.Position, 1.f), matWVP);
    Out.Normal = normalize(mul(float4(In.Normal, 0.f), g_WorldMatrix));

    Out.Texcoord = In.Texcoord;
    Out.WorldPos = mul(float4(In.Position, 1.f), g_WorldMatrix);
    Out.ProjPos = Out.Position;

    return Out;
}

struct PS_IN
{
    float4 Position : SV_POSITION;
    float4 Normal   : NORMAL;
    float3 Tangent  : TANGENT;
    float3 Binormal : BINORMAL;
    float2 Texcoord : TEXCOORD0;
    float4 WorldPos : TEXCOORD1;
    float4 ProjPos  : TEXCOORD2;
};

struct PS_OUT
{
    vector Diffuse  : SV_TARGET0;   // BaseColor
    vector Normal   : SV_TARGET1;   // Normal
    vector Depth    : SV_TARGET2;   // Depth
    vector Material : SV_TARGET3;   // AO, Roughness, Metalness
    vector Emissive  : SV_TARGET4;   // HDR Emissive
};

struct PS_OUT_ID
{
    vector Color : SV_TARGET0;
};

struct PS_OUT_SHADOW
{
    vector LightDepth : SV_TARGET0;
};

PS_OUT PS_Opaque(PS_IN In)
{
    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);

    float4 baseColor = SampleBaseColor(finalUV);

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);

    Out.Diffuse = baseColor;
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);

    float3 orm = SampleOrm(finalUV);
    Out.Material = float4(orm.xyz, 1.f);

    float3 emissive = SampleEmissive(finalUV);
    Out.Emissive = float4(emissive.xyz, 1.f);

    return Out;
}

PS_OUT PS_Mask(PS_IN In)
{
    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);

    float4 baseColor = SampleBaseColor(finalUV);

    float mask = GetOpacityMaskForMasked(finalUV, baseColor);
    if (mask < g_OpacityMaskClipValue)
        discard;

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);

    Out.Diffuse = baseColor;
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);

    float3 orm = SampleOrm(finalUV);
    Out.Material = float4(orm.xyz, 1.f);

    float3 emissive = SampleEmissive(finalUV);
    Out.Emissive = float4(emissive.xyz, 1.f);

    return Out;
}

PS_OUT_SHADOW PS_Shadow(PS_IN In)
{
    PS_OUT_SHADOW Out;

    Out.LightDepth = vector(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_LightFar, 0.f, 1.f);

    return Out;
}

PS_OUT PS_NoTexture(PS_IN In)
{
    PS_OUT Out;

    Out.Diffuse = vector(0.8f, 0.8f, 0.8f, 1.f);
    Out.Normal = vector(In.Normal.xyz * 0.5f + 0.5f, 1.f);
    Out.Depth = vector(In.Position.z, In.Position.w / g_Far, 0.f, 0.f);

    float3 orm = SampleOrm(In.Texcoord);
    Out.Material = float4(orm.xyz, 1.f);

    float3 emissive = SampleEmissive(In.Texcoord);
    Out.Emissive = float4(emissive.xyz, 1.f);

    return Out;
}

PS_OUT_ID PS_ID(PS_IN In)
{
    PS_OUT_ID Out;

    Out.Color.r = float(g_iObjectID & 0xFF) / 255.f;
    Out.Color.g = float((g_iObjectID >> 8) & 0xFF) / 255.f;
    Out.Color.b = float((g_iObjectID >> 16) & 0xFF) / 255.f;
    Out.Color.a = 1.f;

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
