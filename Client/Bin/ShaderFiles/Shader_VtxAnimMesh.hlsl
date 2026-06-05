#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Material.hlsl"
#include "Engine_Shader_Feature.hlsl"
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

struct VS_SHADOW_OUT
{
    float4 Position     : SV_POSITION;
    float2 Texcoord     : TEXCOORD0;
    float4 ProjPos      : TEXCOORD1;
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

VS_SHADOW_OUT VS_Shadow(VS_IN In)
{
    VS_SHADOW_OUT Out;

    float fWeightW = 1.f - (In.BlendWeight.x + In.BlendWeight.y + In.BlendWeight.z);

    float4x4 BoneMatrix =
        g_BoneMatricesBuffer[In.BlendIndex.x].BoneMatrix * In.BlendWeight.x +
        g_BoneMatricesBuffer[In.BlendIndex.y].BoneMatrix * In.BlendWeight.y +
        g_BoneMatricesBuffer[In.BlendIndex.z].BoneMatrix * In.BlendWeight.z +
        g_BoneMatricesBuffer[In.BlendIndex.w].BoneMatrix * fWeightW;

    vector Position = mul(float4(In.Position, 1.f), BoneMatrix);

    float4x4 matWV = mul(g_WorldMatrix, g_LightViewMatrix);
    float4x4 matWVP = mul(matWV, g_LightProjMatrix);

    Out.Position = mul(Position, matWVP);
    Out.Texcoord = In.Texcoord;
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
    vector Emissive  : SV_TARGET4;  // HDR Emissive
};

PS_OUT PS_Opaque(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);
    float dissolveEdge = 0.f;

    if (IsSwitchDissolveEnabled())
    {
        dissolveEdge = ApplySwitchDissolveClip(finalUV);
    }

    // BaseColor
    float4 baseColor = SampleBaseColor(finalUV);
    // Normal
    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);

    Out.Diffuse = baseColor;
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);

    // x = ndc depth(z 분할 후), y = view-space z(0 ~ 1 정규화)
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);

    float3 orm = SampleOrm(finalUV);
    Out.Material = float4(orm.xyz, 1.f); 

    float3 emissive = SampleEmissive(finalUV);
    emissive += dissolveEdge * g_FeatureParam1.rgb * 0.45f;

    if (HasPreviewRim())
    {
        float3 viewDir = normalize(g_CamPosition.xyz - In.WorldPos.xyz);
        float3 rimNormal = normalize(In.Normal.xyz);
        float rim = ComputePreviewRim(rimNormal, viewDir, g_FeatureParam3.x);
        emissive += GetPreviewRimColor() * (rim * GetPreviewRimIntensity());
    }

    Out.Emissive = float4(emissive.xyz, 1.f);

    return Out;
}

PS_OUT PS_Mask(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);
    float dissolveEdge = 0.f;
    // BaseColor
    float4 baseColor = SampleBaseColor(finalUV);
    // Mask
    float mask = GetOpacityMaskForMasked(finalUV, baseColor);
    if (mask < g_OpacityMaskClipValue)
        discard;

    if (IsSwitchDissolveEnabled())
    {
        dissolveEdge = ApplySwitchDissolveClip(finalUV);
    }

    // Normal
    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);

    Out.Diffuse = baseColor;
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);

    float3 orm = SampleOrm(finalUV);
    Out.Material = float4(orm.xyz, 1.f);

    float3 emissive = SampleEmissive(finalUV);
    emissive += dissolveEdge * g_FeatureParam1.rgb * 0.45f;

    if (HasPreviewRim())
    {
        float3 viewDir = normalize(g_CamPosition.xyz - In.WorldPos.xyz);
        float3 rimNormal = normalize(In.Normal.xyz);
        float rim = ComputePreviewRim(rimNormal, viewDir, g_FeatureParam3.x);
        emissive += GetPreviewRimColor() * (rim * GetPreviewRimIntensity());
    }

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

struct PS_OUT_SHADOW
{
    float LightDepth : SV_TARGET0;
};

PS_OUT_SHADOW PS_Shadow(VS_SHADOW_OUT In)
{
    PS_OUT_SHADOW Out;

    Out.LightDepth = In.ProjPos.z / In.ProjPos.w;

    return Out;
}

PS_OUT PS_CharacterEye(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);
    float dissolveEdge = 0.f;

    if (IsSwitchDissolveEnabled())
        dissolveEdge = ApplySwitchDissolveClip(finalUV);

    EyeMaterialSample eye = SampleEyeMaterial(finalUV);

    if (eye.opacity < g_OpacityMaskClipValue)
        discard;

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);
    float3 eyeBaseColor = eye.baseColor;

    // if (HasPreviewRim())
    // {
    //     // SkillTree preview에서는 얼굴이 눌리기 쉬워서 눈만 전체 밝기를 조금 더 올린다.
    //     eyeBaseColor = eyeBaseColor * 1.95f + 0.085f;
    // }

    Out.Diffuse = float4(eyeBaseColor, 1.f);
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
    emissive += dissolveEdge * g_FeatureParam1.rgb * 0.45f;

    if (HasPreviewRim())
    {
        emissive += eyeBaseColor * 0.34f;
    }

    Out.Emissive = float4(emissive.xyz, 1.f);

    return Out;
}

PS_OUT PS_CharacterHair(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);

    float hairMask = SampleHairMask(finalUV);
    float3 viewDir = normalize(g_CamPosition.xyz - In.WorldPos.xyz);
    float facing = saturate(abs(dot(normalize(In.Normal.xyz), viewDir)));
    float edgeFade = smoothstep(0.10f, 0.38f, facing);

    // 본체 coverage를 더 넓게 잡는다.
    const float hairCoreStart = 0.25f;
    const float hairCoreEnd = 0.50f;

    float coreMask = smoothstep(hairCoreStart, hairCoreEnd, hairMask);
    coreMask *= lerp(0.72f, 1.0f, edgeFade);

    // 기존보다 완화해서 더 많은 본체가 남게 한다.
    clip(coreMask - 0.15f);

    float3 hairBSDFColor = Compute_BlenderHairColor(finalUV);

    const float hairAO = 2.f;
    const float hairRoughness = 0.8f;
    const float hairMetalness = 0.f;

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);

    Out.Diffuse = float4(hairBSDFColor, 1.f);
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(hairAO, hairRoughness, hairMetalness, 1.f);

    float3 emissive = float3(0.f, 0.f, 0.f);
    if (HasPreviewRim())
    {
        float3 rimNormal = normalize(In.Normal.xyz);
        float rim = ComputePreviewRim(rimNormal, viewDir, g_FeatureParam3.x);
        float edgeMask = saturate(1.f - coreMask);
        float hairRim = rim * saturate(hairMask) * lerp(0.35f, 1.0f, edgeMask);
        emissive += GetPreviewRimColor() * (hairRim * GetPreviewRimIntensity() * GetPreviewHairRimIntensity());
    }

    Out.Emissive = float4(emissive, 1.f);

    return Out;
}

struct PS_OUT_HAIR_BLEND
{
    vector Color : SV_TARGET0;
};

PS_OUT_HAIR_BLEND PS_CharacterHairBlend(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT_HAIR_BLEND Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);

    float3 hairColor = Compute_BlenderHairColor(finalUV);
    float3 viewDir = normalize(g_CamPosition.xyz - In.WorldPos.xyz);
    float facing = saturate(abs(dot(normalize(In.Normal.xyz), viewDir)));
    float edgeFade = smoothstep(0.10f, 0.38f, facing);

    float hairMask = g_MaterialTextures[MATERIAL_SLOT_OPACITY_MASK].Sample(LinearWrapSampler, finalUV).r;
    hairMask = saturate(hairMask);

    // GBuffer와 같은 core 해석을 사용
    float hairMaskCore = pow(hairMask, 0.75f);

    const float hairCoreStart = 0.20f;
    const float hairCoreEnd = 0.80f;

    float coreMask = smoothstep(hairCoreStart, hairCoreEnd, hairMaskCore);

    // soft는 아주 얇은 가장자리만 담당
    const float hairSoftStart = 0.02f;
    const float hairSoftEnd = 0.55f;

    float softMask = smoothstep(hairSoftStart, hairSoftEnd, hairMask);

    // core는 이미 GBuffer에서 그렸으니 제외
    float hairAlpha = softMask * (1.f - coreMask);
    hairAlpha *= lerp(0.45f, 1.0f, edgeFade);

    // 너무 약한 알파는 버림
    clip(hairAlpha);

    // soft hair는 너무 세면 노이즈가 되므로 적당히만
    hairAlpha *= 0.18f;

    // 실제 테스트에서는 너무 어둡게 누르지 말 것
    float3 blendHairColor = hairColor;

    Out.Color = float4(blendHairColor, hairAlpha);

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(OpaquePass,            RS_Default,     DSS_Default, BS_Default,        VS_Main,   PS_Opaque)          // 0
    PASS_RS_DS_BS_VP(MaskedPass,            RS_Default,     DSS_Default, BS_Default,        VS_Main,   PS_Mask)            // 1
    PASS_RS_DS_BS_VP(OpaqueTwoSidedPass,    RS_CullNone,    DSS_Default, BS_Default,        VS_Main,   PS_Opaque)          // 2
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass,    RS_CullNone,    DSS_Default, BS_Default,        VS_Main,   PS_Mask)            // 3
    PASS_RS_DS_BS_VP(ShadowPass,            RS_Default,     DSS_Default, BS_Default,        VS_Shadow, PS_Shadow)          // 4
    PASS_RS_DS_BS_VP(CharacterEye,          RS_Default,     DSS_Default, BS_Default,        VS_Main,   PS_CharacterEye)      // 5
    PASS_RS_DS_BS_VP(CharacterHair,         RS_CullNone,    DSS_Default, BS_Default,        VS_Main,   PS_CharacterHair)     // 6
    PASS_RS_DS_BS_VP(IDPass,                RS_Default,     DSS_Default, BS_Default,        VS_Main,   PS_ID)                // 7
    PASS_RS_DS_BS_VP(CharacterHairBlend,    RS_CullNone,    DSS_DepthRead, BS_AlphaBlend,   VS_Main,   PS_CharacterHairBlend) // 8
}

