#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Material.hlsl"
#include "Engine_Shader_Utils.hlsl"

float4x4 g_PreTransformMatrix;
uint g_iObjectID;

struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float3 vTangent : TANGENT;
    float3 vBinormal : BINORMAL;
    float2 vTexcoord : TEXCOORD;

    float4 vWorld0 : INST_WORLD0;
    float4 vWorld1 : INST_WORLD1;
    float4 vWorld2 : INST_WORLD2;
    float4 vWorld3 : INST_WORLD3;
    float4 vInstanceColor : INST_COLOR0;
    float2 vLifeTime : INST_LIFE0;
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
    float4 vInstanceColor : COLOR0;
    float2 vLifeTime : TEXCOORD3;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;
    float4x4 mWorld = float4x4(In.vWorld0, In.vWorld1, In.vWorld2, In.vWorld3);
    float4x4 mPreWorld = mul(g_PreTransformMatrix, mWorld);

    Out.vPosition = mul(float4(In.vPosition, 1.f), mul(mPreWorld, mul(g_ViewMatrix, g_ProjMatrix)));
    Out.vNormal = float4(normalize(mul(float4(In.vNormal, 0.f), mPreWorld)).xyz, 0.f);
    Out.vTangent = normalize(mul(float4(In.vTangent, 0.f), mPreWorld)).xyz;
    Out.vBinormal = normalize(mul(float4(In.vBinormal, 0.f), mPreWorld)).xyz;
    Out.vTexcoord = In.vTexcoord;
    Out.vWorldPos = mul(float4(In.vPosition, 1.f), mPreWorld);
    Out.vProjPos = Out.vPosition;
    Out.vInstanceColor = In.vInstanceColor;
    Out.vLifeTime = In.vLifeTime;

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
    float4 vInstanceColor : COLOR0;
    float2 vLifeTime : TEXCOORD3;
};

struct PS_OUT
{
    vector vDiffuse : SV_TARGET0;
    vector vNormal : SV_TARGET1;
    vector vDepth : SV_TARGET2;
    vector vMaterial : SV_TARGET3;
    vector vEmissive : SV_TARGET4;
};

struct PS_OUT_ID
{
    vector vColor : SV_TARGET0;
};

void ApplyInstanceColor(inout float4 baseColor, float4 instanceColor)
{
    baseColor.rgb *= instanceColor.rgb * instanceColor.a;
    baseColor.a *= instanceColor.a;
}

PS_OUT PS_Opaque(PS_IN In)
{
    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.vTexcoord);
    float4 baseColor = SampleBaseColor(finalUV);
    ApplyInstanceColor(baseColor, In.vInstanceColor);

    float3 worldNormal = ComputeWorldNormal(In.vNormal.xyz, In.vTangent, In.vBinormal, finalUV);
    float3 orm = SampleOrm(finalUV);
    float3 emissive = SampleEmissive(finalUV);
    baseColor.rgb = baseColor.rgb * orm.r + emissive;

    Out.vDiffuse = baseColor;
    Out.vNormal = float4(EncodeNormal(worldNormal), 1.f);
    Out.vDepth = float4(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_Far, 0.f, 1.f);
    Out.vMaterial = float4(orm.xyz, 1.f);
    Out.vEmissive = float4(emissive.xyz, 1.f);

    return Out;
}

PS_OUT PS_Mask(PS_IN In)
{
    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.vTexcoord);
    float4 baseColor = SampleBaseColor(finalUV);
    ApplyInstanceColor(baseColor, In.vInstanceColor);

    float mask = GetOpacityMaskForMasked(finalUV, baseColor);
    if (mask * baseColor.a < g_OpacityMaskClipValue)
        discard;

    float3 worldNormal = ComputeWorldNormal(In.vNormal.xyz, In.vTangent, In.vBinormal, finalUV);
    float3 orm = SampleOrm(finalUV);
    float3 emissive = SampleEmissive(finalUV);
    baseColor.rgb = baseColor.rgb * orm.r + emissive;

    Out.vDiffuse = baseColor;
    Out.vNormal = float4(EncodeNormal(worldNormal), 1.f);
    Out.vDepth = float4(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_Far, 0.f, 1.f);
    Out.vMaterial = float4(orm.xyz, 1.f);
    Out.vEmissive = float4(emissive.xyz, 1.f);

    return Out;
}

PS_OUT PS_NoTexture(PS_IN In)
{
    PS_OUT Out;
    float4 baseColor = float4(0.8f, 0.8f, 0.8f, 1.f);
    ApplyInstanceColor(baseColor, In.vInstanceColor);

    Out.vDiffuse = baseColor;
    Out.vNormal = vector(In.vNormal.xyz * 0.5f + 0.5f, 1.f);
    Out.vDepth = vector(In.vPosition.z, In.vPosition.w / g_Far, 0.f, 0.f);
    Out.vMaterial = float4(1.f, 1.f, 0.f, 1.f);
    Out.vEmissive = float4(0.f, 0.f, 0.f, 1.f);

    return Out;
}

PS_OUT PS_OpaqueAR(PS_IN In)
{
    PS_OUT Out;
    float2 finalUV = ApplyUVTransform(In.vTexcoord);

    float4 arTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, finalUV);
    float4 baseColor = float4(arTex.rgb * g_BaseColorTint.rgb, 1.f);
    ApplyInstanceColor(baseColor, In.vInstanceColor);

    float3 worldNormal = ComputeWorldNormal(In.vNormal.xyz, In.vTangent, In.vBinormal, finalUV);
    float3 orm = float3(g_AoScale, arTex.a * g_RoughnessScale, g_MetalnessScale);
    float3 emissive = SampleEmissive(finalUV);
    baseColor.rgb = baseColor.rgb * orm.r + emissive;

    Out.vDiffuse = baseColor;
    Out.vNormal = float4(EncodeNormal(worldNormal), 1.f);
    Out.vDepth = float4(In.vProjPos.z / In.vProjPos.w, In.vProjPos.w / g_Far, 0.f, 1.f);
    Out.vMaterial = float4(orm.xyz, 1.f);
    Out.vEmissive = float4(emissive.xyz, 1.f);
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
    PASS_RS_DS_BS_VP(DefaultPass, RS_Default, DSS_Default, BS_Default, VS_MAIN, PS_Opaque)
    PASS_RS_DS_BS_VP(ShadowPass, RS_Default, DSS_Default, BS_Default, VS_MAIN, PS_Opaque)
    PASS_RS_DS_BS_VP(NoTexturePass, RS_Default, DSS_Default, BS_Default, VS_MAIN, PS_NoTexture)
    PASS_RS_DS_BS_VP(MaskedPass, RS_Default, DSS_Default, BS_Default, VS_MAIN, PS_Mask)
    PASS_RS_DS_BS_VP(TwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_MAIN, PS_Opaque)
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_MAIN, PS_Mask)
    PASS_RS_DS_BS_VP(IDPass, RS_Default, DSS_Default, BS_Default, VS_MAIN, PS_ID)
    PASS_RS_DS_BS_VP(OpaqueARPass, RS_Default, DSS_Default, BS_Default, VS_MAIN, PS_OpaqueAR)
    PASS_RS_DS_BS_VP(OpaqueARTwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_MAIN, PS_OpaqueAR)
}
