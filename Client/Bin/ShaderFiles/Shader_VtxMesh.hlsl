#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Material.hlsl"
#include "Engine_Shader_Feature.hlsl"
#include "Engine_Shader_Utils.hlsl"

uint g_iObjectID;
float4 g_WeaponGhostColor;

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
    bool IsFrontFace : SV_IsFrontFace;
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
    float LightDepth : SV_TARGET0;
};

PS_OUT PS_Opaque(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);
    float dissolveEdge = 0.f;

    if (IsSwitchDissolveEnabled())
        dissolveEdge = ApplySwitchDissolveClip(finalUV);

    float4 baseColor = SampleBaseColor(finalUV);

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);

    Out.Diffuse = baseColor;
    //Out.Diffuse = float4(worldNormal * 0.5f + 0.5f, 1.f);
    
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

PS_OUT PS_Mask(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;

    float2 finalUV = ApplyUVTransform(In.Texcoord);
    float dissolveEdge = 0.f;

    float4 baseColor = SampleBaseColor(finalUV);

    float mask = GetOpacityMaskForMasked(finalUV, baseColor);
    if (mask < g_OpacityMaskClipValue)
        discard;

    if (IsSwitchDissolveEnabled())
        dissolveEdge = ApplySwitchDissolveClip(finalUV);

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

PS_OUT_SHADOW PS_Shadow(PS_IN In)
{
    PS_OUT_SHADOW Out;

    Out.LightDepth = In.ProjPos.z / In.ProjPos.w;

    return Out;
}

PS_OUT PS_NoTexture(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

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
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;
    float2 finalUV = ApplyUVTransform(In.Texcoord);
    float dissolveEdge = 0.f;

    if (IsSwitchDissolveEnabled())
        dissolveEdge = ApplySwitchDissolveClip(finalUV);

    float4 arTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, finalUV);
    float4 baseColor = float4(arTex.rgb * g_BaseColorTint.rgb, 1.f);

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);

    float3 orm = float3(g_AoScale, saturate(arTex.a * g_RoughnessScale), g_MetalnessScale);

    float3 emissive = SampleEmissive(finalUV);
    emissive += dissolveEdge * g_FeatureParam1.rgb * 0.45f;

    if (HasPreviewRim())
    {
        float3 viewDir = normalize(g_CamPosition.xyz - In.WorldPos.xyz);
        float3 rimNormal = normalize(In.Normal.xyz);
        float rim = ComputePreviewRim(rimNormal, viewDir, g_FeatureParam3.x);
        emissive += GetPreviewRimColor() * (rim * GetPreviewRimIntensity());
    }

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
    ApplyPlayerNearClip(In.WorldPos);

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
    if (HasPreviewRim())
    {
        float3 viewDir = normalize(g_CamPosition.xyz - In.WorldPos.xyz);
        float3 rimNormal = normalize(In.Normal.xyz);
        float rim = ComputePreviewRim(rimNormal, viewDir, g_FeatureParam3.x);
        emissive += GetPreviewRimColor() * (rim * GetPreviewRimIntensity());
    }

    float4 baseColor = float4(arTex.rgb * g_BaseColorTint.rgb, 1.f);

    Out.Diffuse = baseColor;
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(orm.xyz, 1.f);
    Out.Emissive = float4(emissive.xyz, 1.f);
    return Out;
}

PS_OUT PS_SignDecal(PS_IN In)
{
    PS_OUT Out;
    float2 uv = ApplyUVTransform(In.Texcoord);
    float4 tex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, uv);

    float brightness = dot(tex.rgb, float3(0.299f, 0.587f, 0.114f));
    clip(brightness - 0.05f);

    float3 color = tex.rgb * g_BaseColorTint.rgb;

    Out.Diffuse = float4(color, 1.f);
    Out.Normal = float4(EncodeNormal(normalize(In.Normal.xyz)), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(1.f, 1.f, 0.f, 1.f);
    Out.Emissive = float4(color * g_EmissiveIntensity, 1.f);
    return Out;
}

PS_OUT PS_LandscapeBlend(PS_IN In)
{
    PS_OUT Out;

    // temporary using slot
    float tileSize = 238.636f;
    float2 weightmapUV = saturate((In.WorldPos.xz - g_UVOffset.xy) / tileSize);
    float4 wm = g_MaterialTextures[MATERIAL_SLOT_HAIR_ID].Sample(LinearWrapSampler, weightmapUV);

    float2 layerUV = In.WorldPos.xz * 0.01f * g_UVScale;

    float4 layer1AR = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, layerUV);
    float4 layer2AR = g_MaterialTextures[MATERIAL_SLOT_HAIR_DEPTH].Sample(LinearWrapSampler, layerUV);

    float w1 = dot(wm, g_BaseColorTint);
    float w2 = dot(wm, g_EmissiveColor);
    float wTotal = w1 + w2;
    float3 blendedColor = lerp(layer1AR.rgb, (layer1AR.rgb * w1 + layer2AR.rgb * w2) / max(wTotal, 0.0001f), saturate(wTotal * 1000.f));

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, layerUV);
    
    Out.Diffuse = float4(blendedColor, 1.f);
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(1.f, 0.8f, 0.f, 1.f);
    Out.Emissive = float4(0.f, 0.f, 0.f, 1.f);

    return Out;
}

PS_OUT_ID PS_ID(PS_IN In)
{
    PS_OUT_ID Out;

    Out.Color.r = float(g_iObjectID & 0xFF) / 255.f;
    Out.Color.g = float((g_iObjectID >> 8) & 0xFF) / 255.f;
    Out.Color.b = float((g_iObjectID >> 16) & 0xFF) / 255.f;
    Out.Color.a = 0.f;

    return Out;
}

PS_OUT PS_FaceAccessoryBeard(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;

    // 수염 카드 주변을 넓게 샘플링할 좌표를 준비한다.
    float2 centerUV = ApplyUVTransform(In.Texcoord);
    BeardSampleCoords coords = BuildBeardSampleCoords(centerUV);

    // 여러 위치의 수염 텍스처를 한 번에 읽어 풍부함 계산에 재사용한다.
    BeardTextureSamples samples = SampleBeardTextures(coords);

    // 수염 두께를 결정하는 고정 coverage를 계산한다.
    float baseCoverage = ComputeBeardCoverage(coords, samples);
    if (baseCoverage < 0.02f)
        discard;

    // 노멀은 표면 방향 출력만 담당하고, 두께에는 더 이상 관여하지 않는다.
    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, centerUV);
    if (!In.IsFrontFace)
        worldNormal *= -1.f;

    // coverage와 같은 범위의 밀도를 따라가며 색 끊김을 줄인다.
    float density = ComputeBeardDensity(samples);

    // 살아남은 픽셀을 하나의 짙은 수염 톤으로 묶는다.
    float3 finalBeardColor = ComputeBeardColor(density);

    // 수염 풍부함을 더 만지려면 helper 쪽 coverage와 density 함수를 먼저 조정한다.
    Out.Diffuse = float4(finalBeardColor, baseCoverage);
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(1.f, g_HairRoughness, 0.f, 1.f);
    Out.Emissive = float4(0.f, 0.f, 0.f, 1.f);

    return Out;
}

PS_OUT PS_FaceAccAlphaBlend(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;
    float2 finalUV = ApplyUVTransform(In.Texcoord);
    float dissolveEdge = 0.f;

    if (IsSwitchDissolveEnabled())
        dissolveEdge = ApplySwitchDissolveClip(finalUV);

    float4 arTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, finalUV);
    float4 baseColor = float4(arTex.rgb * g_BaseColorTint.rgb, 1.f);
    baseColor.a = g_BaseColorTint.a;

    float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);

    float3 orm = float3(g_AoScale, saturate(arTex.a * g_RoughnessScale), g_MetalnessScale);

    float3 emissive = SampleEmissive(finalUV);
    emissive += dissolveEdge * g_FeatureParam1.rgb * 0.45f;

    Out.Diffuse = baseColor;
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(orm.xyz, 1.f);
    Out.Emissive = float4(emissive.xyz, 1.f);
    return Out;
}

PS_OUT PS_WeaponGhost(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;

    float3 worldNormal = normalize(In.Normal.xyz);
    float alpha = saturate(g_WeaponGhostColor.a);
    float3 color = saturate(g_WeaponGhostColor.rgb);

    Out.Diffuse = float4(color, alpha);
    Out.Normal = float4(EncodeNormal(worldNormal), alpha);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, alpha);
    Out.Material = float4(1.f, 0.35f, 0.f, alpha);
    Out.Emissive = float4(color * 0.35f, alpha);

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DefaultPass,               RS_Default,  DSS_Default, BS_Default, VS_Default, PS_Opaque)            // 0
    PASS_RS_DS_BS_VP(ShadowPass,                RS_Default,  DSS_Default, BS_Default, VS_Shadow,  PS_Shadow)            // 1
    PASS_RS_DS_BS_VP(NoTexturePass,             RS_Default,  DSS_Default, BS_Default, VS_Default, PS_NoTexture)         // 2
    PASS_RS_DS_BS_VP(MaskedPass,                RS_Default,  DSS_Default, BS_Default, VS_Default, PS_Mask)              // 3
    PASS_RS_DS_BS_VP(TwoSidedPass,              RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_Opaque)            // 4
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass,        RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_Mask)              // 5
    PASS_RS_DS_BS_VP(IDPass,                    RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_ID)                // 6
    PASS_RS_DS_BS_VP(OpaqueARPass,              RS_Default, DSS_Default,  BS_Default, VS_Default, PS_OpaqueAR)          // 7
    PASS_RS_DS_BS_VP(OpaqueARTwoSidedPass,      RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_OpaqueAR)          // 8
    PASS_RS_DS_BS_VP(PackedAR_NMEPass,          RS_Default, DSS_Default, BS_Default, VS_Default, PS_PackedAR_NME)       // 9
    PASS_RS_DS_BS_VP(PackedAR_NMETwoSidedPass,  RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_PackedAR_NME)      // 10
    PASS_RS_DS_BS_VP(LandscapeBlendPass,        RS_Default, DSS_Default, BS_Default, VS_Default, PS_LandscapeBlend)     // 11
    PASS_RS_DS_BS_VP(FaceAccessoryBeardPass,    RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_FaceAccessoryBeard)// 12
    PASS_RS_DS_BS_VP(FaceAccAlphaBlend,         RS_CullNone, DSS_Default, BS_AlphaBlend, VS_Default, PS_FaceAccAlphaBlend) // 13
    PASS_RS_DS_BS_VP(SignDecalPass,             RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_SignDecal)         // 14
    PASS_RS_DS_BS_VP(WeaponGhostPass,           RS_CullNone, DSS_None, BS_AlphaBlend, VS_Default, PS_WeaponGhost)       // 15
}
