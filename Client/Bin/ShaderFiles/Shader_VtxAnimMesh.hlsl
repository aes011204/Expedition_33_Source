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

    float2 baseUV = In.Texcoord;
    float2 irisUV = ApplyUVTransform(In.Texcoord);

    float3 viewDirWS = normalize(g_CamPosition.xyz - In.WorldPos.xyz);
    float2 irisViewOffset = float2(dot(viewDirWS, normalize(In.Tangent.xyz)), dot(viewDirWS, normalize(In.Binormal.xyz)));

    irisUV -= irisViewOffset * 0.1f;


    float dissolveEdge = 0.f;

    if (IsSwitchDissolveEnabled())
        dissolveEdge = ApplySwitchDissolveClip(baseUV);

    EyeMaterialSample eye = SampleEyeMaterial(baseUV, irisUV);

    if (eye.opacity < g_OpacityMaskClipValue)
        discard;

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, baseUV);
    float3 eyeBaseColor = eye.baseColor;

    // 카메라를 정면으로 바라보는 곳은 밝게, 눈알의 옆면과 가장자리는 어둡게 만들어 구 형태를 강조한다.
    float NdotV = saturate(dot(worldNormal, viewDirWS));
    float eyeCurvatureShade = lerp(0.32f, 1.f, pow(NdotV, 0.65f));
    float scleraMask = 1.f - eye.irisArea;

    eyeBaseColor *= lerp(1.f, eyeCurvatureShade, scleraMask);

    Out.Diffuse = float4(eyeBaseColor, 1.f);
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);\

    // 눈 전체를 덮는 매끈한 각막 표면. 값이 낮을수록 반사가 작고 날카롭게 나타난다.
    float3 orm = SampleOrm(baseUV);
    const float corneaRoughness = 0.005f;
    orm.r = max(orm.r, 0.92f);
    orm.g = min(orm.g, corneaRoughness);
    orm.b = 0.f;

    Out.Material = float4(orm.xyz, 1.f);

    // 눈 표면 가장자리에서 나타나는 각막 Fresnel 반사
    float corneaFresnel = pow(1.f - NdotV, 5.f);

    // 과하지 않은 차가운 각막 반사색
    float3 corneaReflection = float3(0.80f, 0.90f, 1.f) * corneaFresnel * 0.10f;

    float3 T = normalize(In.Tangent.xyz);
    float3 B = normalize(In.Binormal.xyz);

    float3 glintNormalWS = normalize(worldNormal + T * 0.32f - B * 0.18f);

    // 날카로운 작은 안광
    float corneaGlint = pow(saturate(dot(viewDirWS, glintNormalWS)), 140.f);
    float glintMask = saturate(0.20f + eye.irisArea * 0.80f);

    // 작은 흰색 안광. 맨 뒤 값을 높이면 안광 커짐
    float3 corneaHighlight = float3(1.f, 0.96f, 0.90f) * corneaGlint * glintMask * 0.35f;
    float3 emissive = corneaReflection + corneaHighlight;
    
    Out.Emissive = float4(emissive, 1.f);

    return Out;
}

PS_OUT PS_CharacterHair(PS_IN In, bool IsFrontFace : SV_IsFrontFace)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);
    float dissolveEdge = 0.f;

    if (IsSwitchDissolveEnabled())
    {
        dissolveEdge = ApplySwitchDissolveClip(finalUV);
    }

    float hairMask = SampleHairMask(finalUV);
    float hairDepth = SampleHairDepth(finalUV);
    float hairIDValue = SampleHairIDValue(finalUV);
    float hairMaskDensity = SampleHairMaskDensity(finalUV);
    float3 viewDir = normalize(g_CamPosition.xyz - In.WorldPos.xyz);

    float facing = saturate(abs(dot(normalize(In.Normal.xyz), viewDir)));
    float edgeFade = smoothstep(0.08f, 0.42f, facing);

    const float hairCoreStart = 0.22f;
    const float hairCoreEnd = 0.50f;

    float coreMask = smoothstep(hairCoreStart, hairCoreEnd, hairMask);
    
    coreMask *= lerp(0.42f, 1.0f, edgeFade);

    // 고립된 얇은 선은 GBuffer 본체 패스에서 더 강하게 제거한다.
    float clumpDensity = smoothstep(0.08f, 0.24f, hairMaskDensity);
    float isolatedStrandWeight = lerp(0.55f, 1.0f, clumpDensity);
    float coverage = coreMask * isolatedStrandWeight;

    clip(coverage - 0.14f);

    float3 hairBSDFColor = Compute_BlenderHairColor(finalUV);

    float3 normalW = IsFrontFace ? In.Normal.xyz : -In.Normal.xyz;
    float3 tangentW = IsFrontFace ? In.Tangent.xyz : -In.Tangent.xyz;
    float3 binormalW = IsFrontFace ? In.Binormal.xyz : -In.Binormal.xyz;

    float3 worldNormal = ComputeWorldNormal(normalW, tangentW, binormalW, finalUV);
    worldNormal = normalize(worldNormal);
    // 카드 평면 느낌을 줄이기 위해 노멀을 약간 카메라 쪽으로 평균화한다.
    worldNormal = normalize(worldNormal);

    const float hairAO = 1.f;
    float hairRoughness = lerp(0.58f, 0.36f, hairDepth);

    // 얇은 부분 roughness 증가를 너무 세게 주지 않는다.
    hairRoughness += (1.f - coreMask) * 0.025f;

    // ID variation도 너무 한쪽으로 밀지 않는다.
    hairRoughness += (hairIDValue - 0.5f) * 0.25f;

    hairRoughness = clamp(hairRoughness, 0.36f, 0.60f);

    const float hairMetalness = 0.f;

    Out.Diffuse = float4(hairBSDFColor, 1.f);
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(hairAO, hairRoughness, hairMetalness, 1.f);

    float3 emissive = float3(0.f, 0.f, 0.f);

    // emissive로 약하게 보강해 실제 머리카락 같은 결을 먼저 만든다.
    float3 tangentWS = normalize(In.Tangent.xyz);
    float tangentView = abs(dot(tangentWS, viewDir));
    float strandSheen = pow(saturate(1.f - tangentView), 10.f);
    strandSheen *= lerp(0.35f, 1.0f, hairDepth);
    strandSheen *= lerp(0.55f, 1.0f, coreMask);
    strandSheen *= saturate(hairMask);

    strandSheen *= lerp(0.45f, 1.0f, clumpDensity);
    emissive += (hairBSDFColor * 0.06f + float3(1.0f, 0.96f, 0.90f) * 0.025f) * strandSheen;

    if (HasPreviewRim())
    {
        float3 rimNormal = normalize(worldNormal);
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
    float dissolveEdge = 0.f;

    if (IsSwitchDissolveEnabled())
    {
        dissolveEdge = ApplySwitchDissolveClip(finalUV);
    }

    float3 hairColor = Compute_BlenderHairColor(finalUV);
    float3 viewDir = normalize(g_CamPosition.xyz - In.WorldPos.xyz);
    float facing = saturate(abs(dot(normalize(In.Normal.xyz), viewDir)));
    float edgeFade = smoothstep(0.10f, 0.38f, facing);

    float hairMask = SampleHairMask(finalUV);
    float hairMaskDensity = SampleHairMaskDensity(finalUV);

    const float hairCoreStart = 0.22f;
    const float hairCoreEnd = 0.50f;

    float coreMask = smoothstep(hairCoreStart, hairCoreEnd, hairMask);

    coreMask *= lerp(0.72f, 1.0f, edgeFade);

    float clumpDensity = smoothstep(0.08f, 0.24f, hairMaskDensity);
    float isolatedStrandWeight = lerp(0.55f, 1.0f, clumpDensity);
    float gbufferCoverage = coreMask * isolatedStrandWeight;

    // soft pass는 GBuffer에서 빠진 낮은 Mask 영역만 담당한다.
    float softMask = smoothstep(0.015f, 0.30f, hairMask);
    float hairAlpha = softMask * (1.f - saturate(gbufferCoverage));

    hairAlpha *= lerp(0.40f, 1.0f, edgeFade);

    // 잔머리는 soft pass에서 완전히 죽이지 않고 약하게 유지한다.
    hairAlpha *= lerp(0.65f, 1.0f, clumpDensity);

    clip(hairAlpha);

    // soft hair는 너무 세면 노이즈가 되므로 적당히만
    hairAlpha *= 0.18f;

    // 실제 테스트에서는 너무 어둡게 누르지 말 것
    float3 blendHairColor = hairColor;

    Out.Color = float4(blendHairColor, hairAlpha);

    return Out;
}

PS_OUT PS_CharacterEyeLash(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);
    float dissolveEdge = 0.f;

    if (IsSwitchDissolveEnabled())
        dissolveEdge = ApplySwitchDissolveClip(finalUV);

    float4 baseColor = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearClampSampler, finalUV);
    baseColor.rgb *= g_BaseColorTint.rgb;
    baseColor.a *= g_BaseColorTint.a;

    float mask = baseColor.a;

    if (HasMaterialTexture(MATERIAL_SLOT_OPACITY_MASK))
    {
        float4 maskTex = g_MaterialTextures[MATERIAL_SLOT_OPACITY_MASK].Sample(LinearClampSampler, finalUV);
        mask = max(mask, ResolveScalarChannel(maskTex, g_OpacityMaskSource));
    }

    float clipBias = max(fwidth(mask) * 0.75f, 0.02f);
    if (mask < (g_OpacityMaskClipValue - clipBias))
        discard;

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);

    Out.Diffuse = float4(baseColor.rgb, 1.f);
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);

    float3 orm = SampleOrm(finalUV);
    Out.Material = float4(orm.xyz, 1.f);

    float3 emissive = SampleEmissive(finalUV);
    emissive += dissolveEdge * g_FeatureParam1.rgb * 0.45f;
    Out.Emissive = float4(emissive, 1.f);

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(OpaquePass,            RS_Default,     DSS_Default,    BS_Default,         VS_Main,    PS_Opaque)          // 0
    PASS_RS_DS_BS_VP(MaskedPass,            RS_Default,     DSS_Default,    BS_Default,         VS_Main,    PS_Mask)            // 1
    PASS_RS_DS_BS_VP(OpaqueTwoSidedPass,    RS_CullNone,    DSS_Default,    BS_Default,         VS_Main,    PS_Opaque)          // 2
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass,    RS_CullNone,    DSS_Default,    BS_Default,         VS_Main,    PS_Mask)            // 3
    PASS_RS_DS_BS_VP(ShadowPass,            RS_Default,     DSS_Default,    BS_Default,         VS_Shadow,  PS_Shadow)          // 4
    PASS_RS_DS_BS_VP(CharacterEye,          RS_Default,     DSS_Default,    BS_Default,         VS_Main,    PS_CharacterEye)      // 5
    PASS_RS_DS_BS_VP(CharacterHair,         RS_CullNone,    DSS_Default,    BS_Default,         VS_Main,    PS_CharacterHair)     // 6
    PASS_RS_DS_BS_VP(IDPass,                RS_Default,     DSS_Default,    BS_Default,         VS_Main,    PS_ID)                // 7
    PASS_RS_DS_BS_VP(CharacterHairBlend,    RS_CullNone,    DSS_DepthRead,  BS_AlphaBlend,      VS_Main,    PS_CharacterHairBlend) // 8
    PASS_RS_DS_BS_VP(CharacterEyeLash,      RS_CullNone,    DSS_Default,    BS_Default,         VS_Main,    PS_CharacterEyeLash) // 9
}

