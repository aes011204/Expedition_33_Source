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

    Out.LightDepth = vector(In.ProjPos.z / In.ProjPos.w, 0.f, 0.f, 1.f);

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

PS_OUT PS_OpaqueAR(PS_IN In)
{
    PS_OUT Out;
    float2 finalUV = ApplyUVTransform(In.Texcoord);

    float4 arTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, finalUV);
    float4 baseColor = float4(arTex.rgb * g_BaseColorTint.rgb, 1.f);

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);

    float3 orm = float3(g_AoScale, arTex.a * g_RoughnessScale, g_MetalnessScale);

    float3 emissive = SampleEmissive(finalUV);
    baseColor.rgb = baseColor.rgb * orm.r + emissive;

    Out.Diffuse = baseColor;
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(orm.xyz, 1.f);
    Out.Emissive = float4(emissive.xyz, 1.f);
    return Out;
}

// AR + NME packed texture pass:
// BaseColor slot은 AR, Normal slot은 NME texture로 사용한다.
// AR.rgb = base color, AR.a = roughness, NME.rg = normal XY, NME.b = metalness, NME.a = emissive mask.
PS_OUT PS_PackedAR_NME(PS_IN In)
{
    PS_OUT Out;
    float2 finalUV = ApplyUVTransform(In.Texcoord);

    float4 arTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, finalUV);
    float4 nmeTex = g_MaterialTextures[MATERIAL_SLOT_NORMAL].Sample(LinearWrapSampler, finalUV);

    float2 normalXY = nmeTex.rg * 2.f - 1.f;
    const bool flipNormalY = false;
    if (flipNormalY)
        normalXY.y = -normalXY.y;

    normalXY *= g_NormalStrength;
    float normalZ = sqrt(saturate(1.f - dot(normalXY, normalXY)));
    float3 normalTS = normalize(float3(normalXY, normalZ));

    float3 n = normalize(In.Normal.xyz);
    float3 t = normalize(In.Tangent);
    float3 b = normalize(In.Binormal);
    float3 worldNormal = normalize(normalTS.x * t + normalTS.y * b + normalTS.z * n);

    float3 orm = float3(
        g_AoScale,
        saturate(arTex.a * g_RoughnessScale),
        saturate(nmeTex.b * g_MetalnessScale));

    float3 emissive = g_EmissiveColor.rgb * g_EmissiveIntensity * nmeTex.a * g_EmissiveMaskMul;
    float4 baseColor = float4(arTex.rgb * g_BaseColorTint.rgb, 1.f);

    Out.Diffuse = baseColor;
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(orm.xyz, 1.f);
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
    PASS_RS_DS_BS_VP(DefaultPass,         RS_Default,  DSS_Default, BS_Default, VS_Default, PS_Opaque)            // 0
    PASS_RS_DS_BS_VP(ShadowPass,          RS_Default,  DSS_Default, BS_Default, VS_Shadow,  PS_Shadow)            // 1
    PASS_RS_DS_BS_VP(NoTexturePass,       RS_Default,  DSS_Default, BS_Default, VS_Default, PS_NoTexture)         // 2
    PASS_RS_DS_BS_VP(MaskedPass,          RS_Default,  DSS_Default, BS_Default, VS_Default, PS_Mask)              // 3
    PASS_RS_DS_BS_VP(TwoSidedPass,        RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_Opaque)            // 4
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass,  RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_Mask)              // 5
    PASS_RS_DS_BS_VP(IDPass,              RS_Default,  DSS_Default, BS_Default, VS_Default, PS_ID)                // 6
    PASS_RS_DS_BS_VP(OpaqueARPass,        RS_Default, DSS_Default,  BS_Default, VS_Default, PS_OpaqueAR)          // 7
    PASS_RS_DS_BS_VP(OpaqueARTwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_OpaqueAR)         // 8
    PASS_RS_DS_BS_VP(PackedAR_NMEPass, RS_Default, DSS_Default, BS_Default, VS_Default, PS_PackedAR_NME)          // 9
    PASS_RS_DS_BS_VP(PackedAR_NMETwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_PackedAR_NME) // 10
}
