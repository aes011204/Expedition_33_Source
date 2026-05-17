#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Material.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Utils.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"

//cbuffer BoneTransform : register(b1)
//{
//    matrix g_BoneMatrices[950];
//}

// 추후 compute shader 기반 animation/skinning 확장을 위해 bone matrices를 SRV로 받는다.
struct BoneMatrixElement
{
    row_major float4x4 BoneMatrix;
};

StructuredBuffer<BoneMatrixElement> g_BoneMatricesBuffer;

//==================================================
// VS
//==================================================
struct VS_IN
{
    float3  Position    : POSITION;
    float3  Normal      : NORMAL;
    float3  Tangent     : TANGENT;
    float3  Binormal    : BINORMAL;
    float2  Texcoord    : TEXCOORD0;
    uint4   BlendIndex  : BLENDINDEX;
    float4  BlendWeight : BLENDWEIGHT;
};

struct VS_OUT
{
    float4  Position    : SV_POSITION;
    float4  Normal      : NORMAL;
    float3  Tangent     : TANGENT;
    float3  Binormal    : BINORMAL;
    float2  Texcoord    : TEXCOORD0;
    float4  WorldPos    : TEXCOORD1;
    float4  ProjPos     : TEXCOORD2;
};

VS_OUT VS_Main(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV, matWVP;

    float fWeightW = 1.f - (In.BlendWeight.x + In.BlendWeight.y + In.BlendWeight.z);

    //float4x4 BoneMatrix = g_BoneMatrices[In.vBlendIndex.x] * In.vBlendWeight.x +
    //    g_BoneMatrices[In.vBlendIndex.y] * In.vBlendWeight.y +
    //    g_BoneMatrices[In.vBlendIndex.z] * In.vBlendWeight.z +
    //    g_BoneMatrices[In.vBlendIndex.w] * fWeightW;

    float4x4 BoneMatrix =
        g_BoneMatricesBuffer[In.BlendIndex.x].BoneMatrix * In.BlendWeight.x +
        g_BoneMatricesBuffer[In.BlendIndex.y].BoneMatrix * In.BlendWeight.y +
        g_BoneMatricesBuffer[In.BlendIndex.z].BoneMatrix * In.BlendWeight.z +
        g_BoneMatricesBuffer[In.BlendIndex.w].BoneMatrix * fWeightW;

    vector Position = mul(float4(In.Position, 1.f), BoneMatrix);
    vector Normal = mul(float4(In.Normal, 0.f), BoneMatrix);
    vector Tangent = mul(float4(In.Tangent, 0.f), BoneMatrix);
    vector Binormal = mul(float4(In.Binormal, 0.f), BoneMatrix);

    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);

    Out.Position = mul(Position, matWVP);
    Out.Normal = normalize(mul(Normal, g_WorldMatrix));

    Out.Tangent = normalize(mul(Tangent, g_WorldMatrix));
    Out.Binormal = normalize(mul(Binormal, g_WorldMatrix));

    Out.Texcoord = In.Texcoord;
    Out.WorldPos = mul(Position, g_WorldMatrix);
    Out.ProjPos = Out.Position;

    return Out;
}

VS_OUT VS_Shadow(VS_IN In)
{
    VS_OUT Out;

    float fWeightW = 1.f - (In.BlendWeight.x + In.BlendWeight.y + In.BlendWeight.z);

    //float4x4 BoneMatrix = g_BoneMatrices[In.vBlendIndex.x] * In.vBlendWeight.x +
    //    g_BoneMatrices[In.vBlendIndex.y] * In.vBlendWeight.y +
    //    g_BoneMatrices[In.vBlendIndex.z] * In.vBlendWeight.z +
    //    g_BoneMatrices[In.vBlendIndex.w] * fWeightW;

    float4x4 BoneMatrix =
        g_BoneMatricesBuffer[In.BlendIndex.x].BoneMatrix * In.BlendWeight.x +
        g_BoneMatricesBuffer[In.BlendIndex.y].BoneMatrix * In.BlendWeight.y +
        g_BoneMatricesBuffer[In.BlendIndex.z].BoneMatrix * In.BlendWeight.z +
        g_BoneMatricesBuffer[In.BlendIndex.w].BoneMatrix * fWeightW;

    vector Position = mul(float4(In.Position, 1.f), BoneMatrix);
    vector Normal = mul(float4(In.Normal, 0.f), BoneMatrix);
    vector Tangent = mul(float4(In.Tangent, 0.f), BoneMatrix);
    vector Binormal = mul(float4(In.Binormal, 0.f), BoneMatrix);

    float4x4 matWV = mul(g_WorldMatrix, g_LightViewMatrix);
    float4x4 matWVP = mul(matWV, g_LightProjMatrix);

    Out.Position = mul(Position, matWVP);
    Out.Normal = normalize(mul(Normal, g_WorldMatrix));
    Out.Tangent = normalize(mul(Tangent, g_WorldMatrix));
    Out.Binormal = normalize(mul(Binormal, g_WorldMatrix));
    Out.Texcoord = In.Texcoord;
    Out.WorldPos = mul(Position, g_WorldMatrix);
    Out.ProjPos = Out.Position;

    return Out;
}

//==================================================
// PS
//==================================================
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

PS_OUT PS_Opaque(PS_IN In)
{
    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);

    // BaseColor
    float4 baseColor = SampleBaseColor(finalUV);
    // Normal
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
    // BaseColor
    float4 baseColor = SampleBaseColor(finalUV);
    // Mask
    float mask = GetOpacityMaskForMasked(finalUV, baseColor);
    if (mask < g_OpacityMaskClipValue)
        discard;
    // Normal
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

// for character picking
uint g_iObjectID;
struct PS_OUT_ID
{
    vector Color : SV_Target0;
};

PS_OUT_ID PS_ID(PS_IN In)
{
    PS_OUT_ID Out;
    Out.Color.r = float(g_iObjectID & 0xFF) / 255.f;
    Out.Color.g = float((g_iObjectID >> 8) & 0xFF) / 255.f;
    Out.Color.b = float((g_iObjectID >> 16) & 0xFF) / 255.f;
    Out.Color.a = 1.f;
    return Out;
}
//

struct PS_OUT_SHADOW
{
    vector LightDepth : SV_TARGET0;
};

PS_OUT_SHADOW PS_Shadow(PS_IN In)
{
    PS_OUT_SHADOW Out;

    Out.LightDepth = vector(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_LightFar, 0.f, 1.f);
    return Out;
}

PS_OUT PS_CharacterEye(PS_IN In)
{
    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);

    EyeMaterialSample eye = SampleEyeMaterial(finalUV);

    if (eye.opacity < g_OpacityMaskClipValue)
        discard;

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);

    Out.Diffuse = float4(eye.baseColor, 1.f);
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    float3 orm = SampleOrm(finalUV);
    // Eye 전용 ORM 보정: iris는 roughness를 낮춰 입체 하이라이트를 살리고,
    // pupil은 거의 무광에 가깝게 유지한다.
    float eyeRoughness = lerp(0.20f, 0.06f, eye.irisArea);
    eyeRoughness = lerp(eyeRoughness, 0.03f, eye.pupilMask);
    orm.g = min(orm.g, eyeRoughness);
    orm.b = min(orm.b, 0.02f); // 눈에서 금속감 방지
    Out.Material = float4(orm.xyz, 1.f);

    float3 emissive = SampleEmissive(finalUV);
    Out.Emissive = float4(emissive.xyz, 1.f);

    return Out;
}

PS_OUT PS_CharacterHair(PS_IN In)
{
    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);

    HairMaterialSample hair = SampleHairMaterial(finalUV);

    if (hair.opacity < 0.003)
        discard;

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);
        
    Out.Diffuse = float4(hair.baseColor, 1.f);                                          
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);                                
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);    

    float3 orm = SampleOrm(finalUV);        
    Out.Material = float4(orm.xyz, 1.f);    

    float3 emissive = SampleEmissive(finalUV);  
    Out.Emissive = float4(emissive.xyz, 1.f);   

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(OpaquePass,            RS_Default,     DSS_Default, BS_Default, VS_Main,   PS_Opaque)          // 0
    PASS_RS_DS_BS_VP(MaskedPass,            RS_Default,     DSS_Default, BS_Default, VS_Main,   PS_Mask)            // 1
    PASS_RS_DS_BS_VP(OpaqueTwoSidedPass,    RS_CullNone,    DSS_Default, BS_Default, VS_Main,   PS_Opaque)          // 2
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass,    RS_CullNone,    DSS_Default, BS_Default, VS_Main,   PS_Mask)            // 3
    PASS_RS_DS_BS_VP(ShadowPass,            RS_Default,     DSS_Default, BS_Default, VS_Shadow, PS_Shadow)          // 4
    PASS_RS_DS_BS_VP(CharacterEye,          RS_Default,     DSS_Default, BS_Default, VS_Main, PS_CharacterEye)      // 5
    PASS_RS_DS_BS_VP(CharacterHair,         RS_CullNone,    DSS_Default, BS_Default, VS_Main, PS_CharacterHair)     // 6
    PASS_RS_DS_BS_VP(IDPass,                RS_Default,     DSS_Default, BS_Default, VS_Main, PS_ID)                // 7
}

