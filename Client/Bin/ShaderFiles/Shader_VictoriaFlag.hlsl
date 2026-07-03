#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Material.hlsl"
#include "Engine_Shader_Feature.hlsl"
#include "Engine_Shader_Utils.hlsl"

Texture2D g_NumberTextures[10];
Texture2D g_NumberAtlasTexture;

static const float kVictoriaFlagFreeMaskCurve = 1.35f;
static const float3 kVictoriaFlagGoldColor = float3(0.58f, 0.43f, 0.18f);
static const float3 kVictoriaFlagGoldHighlightColor = float3(0.82f, 0.64f, 0.30f);

struct VS_IN
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float2 Texcoord : TEXCOORD0;
    uint VertexID : SV_VertexID;
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float4 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float2 Texcoord : TEXCOORD0;
    float4 WorldPos : TEXCOORD1;
    float4 ProjPos : TEXCOORD2;
    float4 VertexColor : TEXCOORD3;
};

float ComputeVictoriaFlagPoleAnchorMask(float2 surfaceUV)
{
    return 1.f - smoothstep(0.05f, 0.23f, surfaceUV.x);
}

float ComputeVictoriaFlagPoleMotionMask(float2 surfaceUV)
{
    return smoothstep(0.025f, 0.20f, surfaceUV.x);
}

float ComputeVictoriaFlagLowerCornerUnfoldMask(float2 surfaceUV)
{
    const float bottomMask = 1.f - smoothstep(0.18f, 0.72f, surfaceUV.y);
    const float freeCornerMask = smoothstep(0.36f, 0.86f, surfaceUV.x);
    return bottomMask * freeCornerMask * (1.f - ComputeVictoriaFlagPoleAnchorMask(surfaceUV));
}

float ComputeVictoriaFlagUpperCornerUnfoldMask(float2 surfaceUV)
{
    const float topMask = smoothstep(0.44f, 0.82f, surfaceUV.y);
    const float freeCornerMask = smoothstep(0.54f, 0.94f, surfaceUV.x);
    return topMask * freeCornerMask * (1.f - ComputeVictoriaFlagPoleAnchorMask(surfaceUV));
}

float2 ComputeVictoriaFlagSurfaceUV(float2 meshUV)
{
    return saturate(float2(meshUV.y, 1.f - meshUV.x / 0.605f));
}

float2 ComputeVictoriaFlagNumberUV(float2 surfaceUV)
{
    return g_FeatureParam2.w < 0.f ? float2(1.f - surfaceUV.x, surfaceUV.y) : surfaceUV;
}

float3 RelaxVictoriaFlagLowerCornerRestPose(float3 localPos, float2 surfaceUV)
{
    const float poleRejectMask = 1.f - ComputeVictoriaFlagPoleAnchorMask(surfaceUV);
    const float lowerMask = (1.f - smoothstep(0.16f, 0.70f, surfaceUV.y)) * smoothstep(0.42f, 0.90f, surfaceUV.x) * poleRejectMask;
    const float lipMask = lowerMask * (1.f - smoothstep(0.02f, 0.45f, surfaceUV.y));
    const float upperMask = ComputeVictoriaFlagUpperCornerUnfoldMask(surfaceUV);
    const float freeSideDirection = g_FeatureParam2.w < 0.f ? 1.f : -1.f;

    float3 relaxedPos = localPos;
    relaxedPos.x = lerp(relaxedPos.x, -8.f, lowerMask * 0.82f);
    relaxedPos.y += lipMask * 10.f;
    relaxedPos.z += freeSideDirection * lipMask * 9.f;
    relaxedPos.x = lerp(relaxedPos.x, -6.f, upperMask * 0.50f);
    relaxedPos.y -= upperMask * 5.f;
    relaxedPos.z += freeSideDirection * upperMask * 8.f;
    return relaxedPos;
}

float SoftLimitVictoriaFlagDelta(float value, float limit)
{
    return value / (1.f + abs(value) / max(limit, 0.001f));
}

float ComputeVictoriaFlagNumberProtectMask(float2 flagUV)
{
    const float2 numberUV = ComputeVictoriaFlagNumberUV(flagUV);
    const float2 digitSize = float2(0.145f, 0.30f) * max(g_FeatureParam2.z, 0.01f);
    const float digitAdvance = max(g_FeatureParam3.x, 0.001f);
    const float totalWidth = digitSize.x + digitAdvance * 2.f;
    const float2 protectHalfSize = float2(totalWidth * 0.5f + digitSize.x * 0.35f, digitSize.y * 0.5f + 0.07f);
    const float2 distToNumber = abs(numberUV - g_FeatureParam2.xy);
    const float protectX = smoothstep(0.f, 0.045f, protectHalfSize.x - distToNumber.x);
    const float protectY = smoothstep(0.f, 0.055f, protectHalfSize.y - distToNumber.y);
    return protectX * protectY;
}

float3 ComputeVictoriaFlagProceduralDelta(float3 localPos, float2 surfaceUV)
{
    const float time = g_FeatureParam0.x * g_FeatureParam0.y;
    const float freeMask = pow(smoothstep(0.08f, 0.96f, surfaceUV.x), kVictoriaFlagFreeMaskCurve);
    const float wave01 = surfaceUV.x;
    const float bottomMask = 1.f - smoothstep(0.18f, 0.72f, surfaceUV.y);
    const float poleMotionMask = ComputeVictoriaFlagPoleMotionMask(surfaceUV);
    const float topDamp = lerp(1.f, 0.82f, smoothstep(8.f, 72.f, localPos.y));
    const float movableMask = freeMask * poleMotionMask * topDamp;
    const float centerBodyMask = smoothstep(0.12f, 0.42f, freeMask) * (1.f - smoothstep(0.78f, 1.f, freeMask)) *
        poleMotionMask * topDamp;
    const float poleAdjacentMask = smoothstep(0.035f, 0.14f, surfaceUV.x) * (1.f - smoothstep(0.26f, 0.44f, surfaceUV.x)) * topDamp;
    const float lowerBoost = lerp(1.f, 1.45f, bottomMask);
    const float tipMask = smoothstep(0.48f, 0.94f, freeMask) * poleMotionMask;
    const float lowerTipMask = tipMask * bottomMask;
    const float lowerCornerMask = ComputeVictoriaFlagLowerCornerUnfoldMask(surfaceUV);
    const float upperCornerMask = ComputeVictoriaFlagUpperCornerUnfoldMask(surfaceUV);
    const float lowerCornerRidgeMask = smoothstep(0.08f, 0.34f, surfaceUV.y) * (1.f - smoothstep(0.48f, 0.78f, surfaceUV.y)) *
        smoothstep(0.48f, 0.88f, freeMask) * poleMotionMask;
    const float lowerFoldMask = bottomMask * smoothstep(0.22f, 0.78f, freeMask) * poleMotionMask;
    const float lowerRidgeMask = smoothstep(6.f, 28.f, -localPos.y) * (1.f - smoothstep(56.f, 86.f, -localPos.y)) *
        smoothstep(0.18f, 0.72f, freeMask) * poleMotionMask;
    const float freeSideDirection = g_FeatureParam2.w < 0.f ? 1.f : -1.f;

    const float broadPhase = time * 1.35f + wave01 * 5.2f + localPos.y * 0.014f;
    const float broadWave = sin(broadPhase);
    const float followWave = sin(broadPhase + 1.2f + wave01 * 1.8f);
    const float slowRoll = sin(time * 0.95f + wave01 * 3.4f - localPos.y * 0.020f);
    const float verticalTravel = sin(time * 1.05f + wave01 * 4.1f + localPos.y * 0.010f);
    const float verticalRoll = sin(time * 0.72f + wave01 * 2.2f - localPos.y * 0.018f);
    const float smallRipple = sin(time * 1.55f + wave01 * 6.0f + localPos.y * 0.016f + broadWave * 0.12f);
    const float foldPhase = broadPhase - 1.1f + freeMask * 0.55f - localPos.y * 0.006f;
    const float foldOpenRaw = 0.5f + 0.5f * sin(foldPhase);
    const float foldOpen = foldOpenRaw * foldOpenRaw * (3.f - 2.f * foldOpenRaw);
    const float ridgeOpenRaw = 0.5f + 0.5f * sin(foldPhase - 0.42f);
    const float ridgeOpen = ridgeOpenRaw * ridgeOpenRaw * (3.f - 2.f * ridgeOpenRaw);
    const float cornerOpenRaw = 0.5f + 0.5f * sin(foldPhase - 0.18f);
    const float cornerOpen = cornerOpenRaw * cornerOpenRaw * (3.f - 2.f * cornerOpenRaw);

    const float bodyOut = (broadWave * 13.0f + followWave * 4.2f + slowRoll * 2.6f) * movableMask * lowerBoost +
        (broadWave * 7.5f + slowRoll * 4.0f) * centerBodyMask;
    const float verticalEdgeMask = smoothstep(0.24f, 0.92f, abs(surfaceUV.y - 0.5f) * 2.f);
    const float verticalHeave = (verticalTravel * 5.2f + verticalRoll * 3.0f) * movableMask * lerp(0.85f, 1.20f, freeMask);
    const float verticalEdgeHeave = (verticalTravel * 2.8f + slowRoll * 1.5f) * movableMask * verticalEdgeMask;
    const float bodyVertical = (followWave * 4.0f + slowRoll * 2.5f) * movableMask * lowerBoost +
        (followWave * 4.5f + broadWave * 2.5f) * centerBodyMask +
        verticalHeave + verticalEdgeHeave;
    const float bodyLength = freeSideDirection * (slowRoll * 0.8f * movableMask + followWave * 1.4f * centerBodyMask);
    const float poleOut = (broadWave * 2.5f + slowRoll * 1.6f) * poleAdjacentMask;
    const float poleVertical = (followWave * 1.6f + slowRoll * 1.2f) * poleAdjacentMask;
    const float poleLength = freeSideDirection * (slowRoll * 0.55f + broadWave * 0.35f) * poleAdjacentMask;

    const float lowerTipOut = (broadWave * 5.0f + followWave * 4.0f + smallRipple * 0.8f) * lowerTipMask;
    const float lowerTipVertical = (broadWave * 5.0f + followWave * 3.0f) * lowerTipMask;
    const float lowerTipLength = freeSideDirection * (foldOpen * 4.0f + abs(followWave) * 1.4f) * lowerTipMask;

    // Open the lower fold vertically: bottom edge moves down while the ridge above it flattens and lifts.
    const float foldFlattenMask = saturate(lowerFoldMask * 0.65f + lowerRidgeMask);
    const float foldFlattenOut = -localPos.x * (foldOpen * 0.55f * lowerFoldMask + ridgeOpen * 1.15f * lowerRidgeMask);
    const float foldOut = (followWave * 1.4f + broadWave * 1.8f) * lowerFoldMask + foldFlattenOut;
    const float foldVertical = -(foldOpen * 13.0f + abs(followWave) * 1.8f) * lowerFoldMask +
        (ridgeOpen * 11.0f + abs(broadWave) * 1.4f) * lowerRidgeMask;
    const float foldLength = freeSideDirection * ((foldOpen * 5.0f + abs(broadWave) * 1.6f) * lowerFoldMask +
        (ridgeOpen * 3.2f + 0.6f) * lowerRidgeMask);
    const float cornerOpenPulse = cornerOpen * (0.72f + abs(followWave) * 0.28f);
    const float cornerBottomEdgeMask = lowerCornerMask * smoothstep(28.f, 82.f, -localPos.y);
    const float cornerUnfoldFlattenMask = saturate(lowerCornerMask + lowerCornerRidgeMask * 0.85f);
    const float cornerFlattenOut = -localPos.x * (3.40f * cornerOpenPulse + 0.35f * abs(broadWave)) * cornerUnfoldFlattenMask;
    const float cornerLipLift = (max(-localPos.y, 0.f) * 0.62f + 24.0f) * cornerOpenPulse * cornerBottomEdgeMask;
    const float cornerRidgeSettle = (cornerOpenPulse * 8.0f + ridgeOpen * 4.0f) * lowerCornerRidgeMask;
    const float cornerVertical = cornerLipLift - cornerRidgeSettle;
    const float cornerLength = freeSideDirection * ((cornerOpenPulse * 18.0f + abs(followWave) * 3.5f) * cornerBottomEdgeMask +
        (cornerOpenPulse * 4.0f + followWave * 0.8f) * lowerCornerRidgeMask);
    const float upperCornerOpenPulse = ridgeOpen * (0.68f + abs(broadWave) * 0.32f);
    const float upperCornerFlattenOut = -localPos.x * (1.85f * upperCornerOpenPulse + abs(followWave) * 0.35f) * upperCornerMask;
    const float upperCornerLift = -(upperCornerOpenPulse * 11.0f + abs(followWave) * 3.0f) * upperCornerMask;
    const float upperCornerLength = freeSideDirection * (upperCornerOpenPulse * 13.0f + abs(slowRoll) * 2.4f) * upperCornerMask;

    const float outOfPlane = bodyOut * (1.f - foldOpen * foldFlattenMask * 0.35f) + poleOut + lowerTipOut + foldOut + cornerFlattenOut + upperCornerFlattenOut;
    const float vertical = bodyVertical + poleVertical + lowerTipVertical + foldVertical + cornerVertical + upperCornerLift;
    const float lengthDrift = bodyLength + poleLength + lowerTipLength + foldLength + cornerLength + upperCornerLength;
    const float3 limitedDelta = float3(
        SoftLimitVictoriaFlagDelta(outOfPlane, 34.f),
        SoftLimitVictoriaFlagDelta(vertical, 32.f),
        SoftLimitVictoriaFlagDelta(lengthDrift, 18.f));

    return limitedDelta * saturate(poleMotionMask + lowerCornerMask + upperCornerMask);
}

float3 ApplyVictoriaFlagDeform(float3 localPos, float2 flagUV)
{
    const float motionScale = g_FeatureParam1.z;
    const float poleAnchorMask = ComputeVictoriaFlagPoleAnchorMask(flagUV);
    const float poleMotionMask = ComputeVictoriaFlagPoleMotionMask(flagUV);
    const float3 relaxedCandidatePos = RelaxVictoriaFlagLowerCornerRestPose(localPos, flagUV);
    const float3 relaxedLocalPos = lerp(relaxedCandidatePos, localPos, poleAnchorMask);
    const float3 straightLocalPos = relaxedLocalPos;
    const float3 proceduralDelta = ComputeVictoriaFlagProceduralDelta(relaxedLocalPos, flagUV);
    const float lowerCornerMask = ComputeVictoriaFlagLowerCornerUnfoldMask(flagUV);
    const float upperCornerMask = ComputeVictoriaFlagUpperCornerUnfoldMask(flagUV);
    const float cornerUnlockMask = saturate(lowerCornerMask + upperCornerMask) * (1.f - poleAnchorMask);

    return straightLocalPos + proceduralDelta * motionScale * saturate(max(poleMotionMask, cornerUnlockMask));
}

float4 SampleFlagBaseColor(float2 flagUV)
{
    const float brightness = g_FeatureParam1.w;
    float3 baseColor = float3(0.075f, 0.075f, 0.07f) * brightness;

    if (HasMaterialTexture(MATERIAL_SLOT_BASECOLOR))
    {
        const float4 flagTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].SampleLevel(LinearWrapSampler, flagUV, 0.f);
        const float texLuma = dot(flagTex.rgb, float3(0.299f, 0.587f, 0.114f));
        const float detailMask = saturate((texLuma - 0.5f) * 1.65f + 0.5f);
        const float3 tintedDetail = saturate(flagTex.rgb * 1.75f);

        baseColor *= lerp(0.52f, 1.48f, detailMask);
        baseColor = lerp(baseColor, baseColor * tintedDetail, 0.42f);
    }

    return float4(saturate(baseColor), 1.f);
}

float4 SampleFlagBaseColorPixel(float2 flagUV)
{
    const float brightness = g_FeatureParam1.w;
    float3 baseColor = float3(0.094f, 0.092f, 0.086f) * brightness;

    if (HasMaterialTexture(MATERIAL_SLOT_BASECOLOR))
    {
        const float4 flagTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, flagUV);
        const float texLuma = dot(flagTex.rgb, float3(0.299f, 0.587f, 0.114f));
        const float detailMask = saturate((texLuma - 0.5f) * 1.40f + 0.5f);
        const float3 tintedDetail = saturate(flagTex.rgb * 1.45f + 0.06f);

        baseColor *= lerp(0.68f, 1.34f, detailMask);
        baseColor = lerp(baseColor, baseColor * tintedDetail, 0.35f);
    }

    return float4(saturate(baseColor), 1.f);
}

float ComputeVictoriaFlagFabricDetail(float2 flagUV)
{
    const float coarse = sin(flagUV.x * 82.f + sin(flagUV.y * 13.f) * 0.35f) *
        sin(flagUV.y * 74.f + sin(flagUV.x * 11.f) * 0.30f);
    const float fine = sin(flagUV.x * 226.f + flagUV.y * 9.f) *
        sin(flagUV.y * 204.f - flagUV.x * 7.f);
    const float microGrid = sin(flagUV.x * 355.f + sin(flagUV.y * 31.f) * 0.20f) *
        sin(flagUV.y * 330.f + sin(flagUV.x * 27.f) * 0.18f);
    const float mottled = sin(dot(flagUV, float2(43.0f, 61.0f))) *
        sin(dot(flagUV, float2(-37.0f, 52.0f)));

    return saturate(0.5f + coarse * 0.135f + fine * 0.070f + microGrid * 0.045f + mottled * 0.070f);
}

float ComputeVictoriaFlagWrinkleRidge(float2 flagUV)
{
    const float poleMask = 1.f - smoothstep(0.05f, 0.42f, flagUV.x);
    const float bodyMask = smoothstep(0.10f, 0.92f, flagUV.x) * (1.f - smoothstep(0.92f, 1.f, flagUV.x));
    const float poleBands = 0.5f + 0.5f * sin(flagUV.y * 39.f + flagUV.x * 16.f);
    const float bodyWaveA = 0.5f + 0.5f * sin(flagUV.y * 14.f + flagUV.x * 19.f + sin(flagUV.x * 7.f) * 0.85f);
    const float bodyWaveB = 0.5f + 0.5f * sin((flagUV.x + flagUV.y * 0.55f) * 25.f);
    const float poleRidge = pow(saturate(poleBands), 5.0f) * poleMask;
    const float bodyRidge = pow(saturate(bodyWaveA * 0.68f + bodyWaveB * 0.32f), 7.0f) * bodyMask;

    return saturate(poleRidge * 0.55f + bodyRidge * 0.65f);
}

float ComputeVictoriaFlagWrinkleShadow(float2 flagUV)
{
    const float shiftedRidge = ComputeVictoriaFlagWrinkleRidge(saturate(flagUV + float2(-0.018f, 0.034f)));
    const float diagonalRidge = ComputeVictoriaFlagWrinkleRidge(saturate(flagUV + float2(0.030f, 0.020f)));
    const float currentRidge = ComputeVictoriaFlagWrinkleRidge(flagUV);

    return saturate(shiftedRidge * 0.58f + diagonalRidge * 0.32f - currentRidge * 0.46f);
}

float4 ApplyVictoriaFlagFabricColor(float2 flagUV, float4 baseColor)
{
    const float fabric = ComputeVictoriaFlagFabricDetail(flagUV);
    const float wrinkleRidge = ComputeVictoriaFlagWrinkleRidge(flagUV);
    const float wrinkleShadow = ComputeVictoriaFlagWrinkleShadow(flagUV);
    const float threadLift = saturate((fabric - 0.5f) * 2.f);

    baseColor.rgb *= lerp(0.86f, 1.14f, fabric);
    baseColor.rgb *= lerp(1.f, 0.53f, wrinkleShadow);
    baseColor.rgb = lerp(baseColor.rgb, baseColor.rgb * float3(1.58f, 1.49f, 1.33f) + float3(0.034f, 0.030f, 0.020f), wrinkleRidge * 0.34f);
    baseColor.rgb += threadLift * 0.008f;
    return float4(saturate(baseColor.rgb), baseColor.a);
}

float3 ApplyVictoriaFlagFabricNormal(float2 flagUV, float3 normalW, float3 tangentW, float3 binormalW)
{
    float3 n = normalize(normalW);
    float3 t = normalize(tangentW - n * dot(tangentW, n));
    float3 b = normalize(binormalW - n * dot(binormalW, n) - t * dot(binormalW, t));

    const float poleMask = 1.f - smoothstep(0.05f, 0.42f, flagUV.x);
    const float bodyMask = smoothstep(0.10f, 0.92f, flagUV.x) * (1.f - smoothstep(0.92f, 1.f, flagUV.x));
    const float poleSlope = cos(flagUV.y * 39.f + flagUV.x * 16.f) * poleMask;
    const float bodySlopeA = cos(flagUV.y * 14.f + flagUV.x * 19.f + sin(flagUV.x * 7.f) * 0.85f) * bodyMask;
    const float bodySlopeB = cos((flagUV.x + flagUV.y * 0.55f) * 25.f) * bodyMask;
    const float fabricSlopeX = cos(flagUV.x * 82.f + sin(flagUV.y * 13.f) * 0.35f) * 0.024f +
        cos(flagUV.x * 355.f + sin(flagUV.y * 31.f) * 0.20f) * 0.036f;
    const float fabricSlopeY = cos(flagUV.y * 74.f + sin(flagUV.x * 11.f) * 0.30f) * 0.022f +
        cos(flagUV.y * 330.f + sin(flagUV.x * 27.f) * 0.18f) * 0.034f;

    const float warpSlope = (fabricSlopeX + bodySlopeA * 0.145f + bodySlopeB * 0.088f + poleSlope * 0.076f) * 1.18f;
    const float weftSlope = (fabricSlopeY + bodySlopeA * 0.106f + bodySlopeB * 0.150f + poleSlope * 0.178f) * 1.18f;

    return normalize(n + t * warpSlope + b * weftSlope);
}

float ComputeVictoriaFlagBorderLine(float edgeDistance, float center, float halfWidth, float softness)
{
    const float aaWidth = max(softness, fwidth(edgeDistance) * 1.5f);
    const float inner = smoothstep(center - halfWidth - aaWidth, center - halfWidth, edgeDistance);
    const float outer = 1.f - smoothstep(center + halfWidth, center + halfWidth + aaWidth, edgeDistance);
    return inner * outer;
}

float4 ApplyVictoriaFlagBorder(float2 flagUV, float4 baseColor)
{
    const float edgeDistance = min(min(flagUV.x, 1.f - flagUV.x), min(flagUV.y, 1.f - flagUV.y));
    const float edgeAA = max(0.002f, fwidth(edgeDistance) * 1.5f);
    const float outerLine = 1.f - smoothstep(0.007f, 0.007f + edgeAA, edgeDistance);
    const float innerLine = ComputeVictoriaFlagBorderLine(edgeDistance, 0.030f, 0.0015f, 0.004f);
    const float borderMask = max(outerLine, innerLine);

    const float borderHighlight = pow(borderMask, 2.2f);
    const float3 borderColor = lerp(kVictoriaFlagGoldColor, kVictoriaFlagGoldHighlightColor, borderHighlight * 0.45f);
    baseColor.rgb = lerp(baseColor.rgb, borderColor, borderMask);
    return baseColor;
}

VS_OUT VS_Default(VS_IN In)
{
    VS_OUT Out;

    const float2 surfaceUV = ComputeVictoriaFlagSurfaceUV(In.Texcoord);
    const float3 flagPosition = ApplyVictoriaFlagDeform(In.Position, surfaceUV);

    const float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    const float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.Position = mul(float4(flagPosition, 1.f), matWVP);
    Out.Normal = float4(normalize(mul(float4(In.Normal, 0.f), g_WorldMatrix)).xyz, 0.f);
    Out.Tangent = normalize(mul(float4(0.f, 1.f, 0.f, 0.f), g_WorldMatrix)).xyz;
    Out.Binormal = normalize(mul(float4(0.f, 0.f, 1.f, 0.f), g_WorldMatrix)).xyz;
    Out.Texcoord = surfaceUV;
    Out.WorldPos = mul(float4(flagPosition, 1.f), g_WorldMatrix);
    Out.ProjPos = Out.Position;
    Out.VertexColor = SampleFlagBaseColor(surfaceUV);

    return Out;
}

VS_OUT VS_Shadow(VS_IN In)
{
    VS_OUT Out;

    const float2 surfaceUV = ComputeVictoriaFlagSurfaceUV(In.Texcoord);
    const float3 flagPosition = ApplyVictoriaFlagDeform(In.Position, surfaceUV);

    const float4x4 matWV = mul(g_WorldMatrix, g_LightViewMatrix);
    const float4x4 matWVP = mul(matWV, g_LightProjMatrix);

    Out.Position = mul(float4(flagPosition, 1.f), matWVP);
    Out.Normal = float4(normalize(mul(float4(In.Normal, 0.f), g_WorldMatrix)).xyz, 0.f);
    Out.Tangent = normalize(mul(float4(0.f, 1.f, 0.f, 0.f), g_WorldMatrix)).xyz;
    Out.Binormal = normalize(mul(float4(0.f, 0.f, 1.f, 0.f), g_WorldMatrix)).xyz;
    Out.Texcoord = surfaceUV;
    Out.WorldPos = mul(float4(flagPosition, 1.f), g_WorldMatrix);
    Out.ProjPos = Out.Position;
    Out.VertexColor = SampleFlagBaseColor(surfaceUV);

    return Out;
}

struct PS_IN
{
    float4 Position : SV_POSITION;
    float4 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float2 Texcoord : TEXCOORD0;
    float4 WorldPos : TEXCOORD1;
    float4 ProjPos : TEXCOORD2;
    float4 VertexColor : TEXCOORD3;
    bool IsFrontFace : SV_IsFrontFace;
};

struct PS_OUT
{
    vector Diffuse : SV_TARGET0;
    vector Normal : SV_TARGET1;
    vector Depth : SV_TARGET2;
    vector Material : SV_TARGET3;
    vector Emissive : SV_TARGET4;
};

struct PS_OUT_SHADOW
{
    float LightDepth : SV_TARGET0;
};

float4 SampleDigitTexture(int digit, float2 uv)
{
    digit = clamp(digit, 0, 9);

    if (digit == 0) return g_NumberTextures[0].Sample(LinearClampSampler, uv);
    if (digit == 1) return g_NumberTextures[1].Sample(LinearClampSampler, uv);
    if (digit == 2) return g_NumberTextures[2].Sample(LinearClampSampler, uv);
    if (digit == 3) return g_NumberTextures[3].Sample(LinearClampSampler, uv);
    if (digit == 4) return g_NumberTextures[4].Sample(LinearClampSampler, uv);
    if (digit == 5) return g_NumberTextures[5].Sample(LinearClampSampler, uv);
    if (digit == 6) return g_NumberTextures[6].Sample(LinearClampSampler, uv);
    if (digit == 7) return g_NumberTextures[7].Sample(LinearClampSampler, uv);
    if (digit == 8) return g_NumberTextures[8].Sample(LinearClampSampler, uv);
    return g_NumberTextures[9].Sample(LinearClampSampler, uv);
}

float4 ApplyNumberOverlay(float2 flagUV, float4 baseColor)
{
    const float2 numberUV = ComputeVictoriaFlagNumberUV(flagUV);
    const int flagNumber = clamp((int)round(g_FeatureParam3.w), 0, 999);
    const int hundredsDigit = flagNumber / 100;
    const int tensDigit = (flagNumber / 10) % 10;
    const int onesDigit = flagNumber % 10;

    const float2 numberCenter = g_FeatureParam2.xy;
    const float2 mirroredNumberUV = float2(numberCenter.x * 2.f - numberUV.x, numberUV.y);
    const float2 digitSize = float2(0.145f, 0.30f) * max(g_FeatureParam2.z, 0.01f);
    const float digitAdvance = max(g_FeatureParam3.x, 0.001f);
    const float totalWidth = digitSize.x + digitAdvance * 2.f;
    const float2 firstMin = numberCenter - float2(totalWidth * 0.5f, digitSize.y * 0.5f);

    float4 outColor = baseColor;
    const float4 atlasFallback = g_NumberAtlasTexture.Sample(LinearClampSampler, flagUV);
    outColor.rgb += atlasFallback.rgb * atlasFallback.a * 0.000001f;

    int digits[3] = { hundredsDigit, tensDigit, onesDigit };
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        const float2 digitMin = firstMin + float2(digitAdvance * (float)i, 0.f);
        const float2 digitUV = (mirroredNumberUV - digitMin) / digitSize;
        if (all(digitUV >= 0.f) && all(digitUV <= 1.f))
        {
            const float4 digitColor = SampleDigitTexture(digits[i], digitUV);
            const float digitMask = saturate(max(digitColor.a, max(digitColor.r, max(digitColor.g, digitColor.b))));
            const float digitHighlight = pow(digitMask, 2.0f);
            const float3 digitGold = lerp(kVictoriaFlagGoldColor, kVictoriaFlagGoldHighlightColor, digitHighlight * 0.40f);
            outColor.rgb = lerp(outColor.rgb, digitGold, digitMask);
        }
    }

    return outColor;
}

PS_OUT PS_Opaque(PS_IN In)
{
    ApplyPlayerNearClip(In.WorldPos);

    PS_OUT Out;

    const float4 flagBaseColor = SampleFlagBaseColorPixel(In.Texcoord);
    const float4 baseColor = ApplyVictoriaFlagFabricColor(In.Texcoord,
        ApplyNumberOverlay(In.Texcoord, ApplyVictoriaFlagBorder(In.Texcoord, flagBaseColor)));
    float3 worldNormal = normalize(In.Normal.xyz);
    if (!In.IsFrontFace)
        worldNormal *= -1.f;
    worldNormal = ApplyVictoriaFlagFabricNormal(In.Texcoord, worldNormal, In.Tangent, In.Binormal);

    Out.Diffuse = baseColor;
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(1.f, 0.94f, 0.f, 1.f);
    Out.Emissive = float4(0.f, 0.f, 0.f, 1.f);

    return Out;
}

PS_OUT_SHADOW PS_Shadow(PS_IN In)
{
    PS_OUT_SHADOW Out;
    Out.LightDepth = In.ProjPos.z / In.ProjPos.w;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DefaultPass, RS_Default, DSS_Default, BS_Default, VS_Default, PS_Opaque) // 0
    PASS_RS_DS_BS_VP(ShadowPass, RS_Default, DSS_Default, BS_Default, VS_Shadow, PS_Shadow) // 1
    PASS_RS_DS_BS_VP(NoTexturePass, RS_Default, DSS_Default, BS_Default, VS_Default, PS_Opaque) // 2
    PASS_RS_DS_BS_VP(MaskedPass, RS_Default, DSS_Default, BS_Default, VS_Default, PS_Opaque) // 3
    PASS_RS_DS_BS_VP(TwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_Opaque) // 4
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_Default, PS_Opaque) // 5
}
