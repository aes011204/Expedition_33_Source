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
Texture2D g_VATPositionTexture;
Texture2D g_VATRotationTexture;
Texture2D g_VATColorTexture;

static const float kVictoriaFlagFreeMaskCurve = 1.35f;
static const float3 kVictoriaFlagGoldColor = float3(0.74f, 0.50f, 0.11f);
static const float3 kVictoriaFlagGoldHighlightColor = float3(1.0f, 0.78f, 0.24f);
static const float kVictoriaFlagVATTextureHeight = 330.f;

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
    return saturate(float2(1.f - meshUV.x, meshUV.y));
}

float2 ComputeVictoriaFlagVATFrameUV(float2 vatUV, float frameIndex)
{
    const float rowsPerFrame = max(g_FeatureParam1.y, 1.f);
    const float frameYOffset = frameIndex * rowsPerFrame / kVictoriaFlagVATTextureHeight;
    return float2(vatUV.x, 1.f - vatUV.y - frameYOffset);
}

float3 RemapVictoriaFlagVATPosition(float3 vatPosition)
{
    return float3(vatPosition.x, vatPosition.z, vatPosition.y);
}

float3 SampleVictoriaFlagVATPosition(float3 fallbackLocalPos, float2 vatUV)
{
    return fallbackLocalPos;

    if (vatUV.y > 0.99f)
        return fallbackLocalPos;

    const float frameCount = max(g_FeatureParam1.x, 1.f);
    const float fps = max(g_FeatureParam0.z, 1.f);
    const float debugFrame = g_FeatureParam0.w;
    const float frameTime = g_FeatureParam0.x * g_FeatureParam0.y * fps;
    const float frame = debugFrame >= 0.f ? clamp(debugFrame, 0.f, frameCount - 1.f) : frac(frameTime / frameCount) * frameCount;
    const float frameA = floor(frame);
    const float frameB = fmod(frameA + 1.f, frameCount);
    const float frameAlpha = debugFrame >= 0.f ? 0.f : frac(frame);

    const float3 basePos = RemapVictoriaFlagVATPosition(g_VATPositionTexture.SampleLevel(PointClampSampler, ComputeVictoriaFlagVATFrameUV(vatUV, 0.f), 0.f).xyz);
    const float3 posA = RemapVictoriaFlagVATPosition(g_VATPositionTexture.SampleLevel(PointClampSampler, ComputeVictoriaFlagVATFrameUV(vatUV, frameA), 0.f).xyz);
    const float3 posB = RemapVictoriaFlagVATPosition(g_VATPositionTexture.SampleLevel(PointClampSampler, ComputeVictoriaFlagVATFrameUV(vatUV, frameB), 0.f).xyz);
    return fallbackLocalPos + (lerp(posA, posB, frameAlpha) - basePos);
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
    return float4(0.075f, 0.075f, 0.07f, 1.f) * float4(brightness, brightness, brightness, 1.f);
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
    const float3 flagPosition = lerp(In.Position, SampleVictoriaFlagVATPosition(In.Position, In.Tangent.xy), saturate(g_FeatureParam1.z));

    const float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    const float4x4 matWVP = mul(matWV, g_ProjMatrix);
    const float3 smoothFlagNormal = normalize(mul(float4(0.f, 0.f, 1.f, 0.f), g_WorldMatrix)).xyz;

    Out.Position = mul(float4(flagPosition, 1.f), matWVP);
    Out.Normal = float4(smoothFlagNormal, 0.f);
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
    const float3 flagPosition = lerp(In.Position, SampleVictoriaFlagVATPosition(In.Position, In.Tangent.xy), saturate(g_FeatureParam1.z));

    const float4x4 matWV = mul(g_WorldMatrix, g_LightViewMatrix);
    const float4x4 matWVP = mul(matWV, g_LightProjMatrix);
    const float3 smoothFlagNormal = normalize(mul(float4(0.f, 0.f, 1.f, 0.f), g_WorldMatrix)).xyz;

    Out.Position = mul(float4(flagPosition, 1.f), matWVP);
    Out.Normal = float4(smoothFlagNormal, 0.f);
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

    const float4 baseColor = ApplyNumberOverlay(In.Texcoord, ApplyVictoriaFlagBorder(In.Texcoord, In.VertexColor));
    float3 worldNormal = normalize(In.Normal.xyz);

    Out.Diffuse = baseColor;
    Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(1.f, 0.65f, 0.f, 1.f);
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
