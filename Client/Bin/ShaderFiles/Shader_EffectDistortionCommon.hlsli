#ifndef SHADER_EFFECT_DISTORTION_COMMON_HLSLI
#define SHADER_EFFECT_DISTORTION_COMMON_HLSLI

float EffectDistortion_ComputeShockwavePixelUvWidth(float2 cardUV)
{
    return max(max(fwidth(cardUV.x), fwidth(cardUV.y)), 0.0001f);
}

float EffectDistortion_ComputeShapeCoverage(float4 shapeParams, float2 cardUV, float lifeProgress)
{
    const float mode = shapeParams.x;
    float shapeCoverage = 1.f;

    if (mode >= 0.5f)
    {
        const float radius = max(shapeParams.y, 0.f);
        const float thickness = max(shapeParams.z, 0.f);
        const float softness = max(shapeParams.w, 0.0001f);
        const float distanceFromCenter = distance(cardUV, float2(0.5f, 0.5f));

        if (mode < 1.5f)
            shapeCoverage = 1.f - smoothstep(radius, radius + softness, distanceFromCenter);
        else if (mode < 2.5f)
        {
            const float ringDistance = abs(distanceFromCenter - radius);
            shapeCoverage = 1.f - smoothstep(thickness, thickness + softness, ringDistance);
        }
        else
        {
            const float pixelUvWidth = EffectDistortion_ComputeShockwavePixelUvWidth(cardUV);
            const float shockwaveThickness = max(thickness, 0.f) * pixelUvWidth;
            const float shockwaveSoftness = max(softness, 0.0001f) * pixelUvWidth;
            const float shockwaveRadiusMax = max(0.f, 0.5f - shockwaveThickness - shockwaveSoftness);
            const float shockwaveRadius = saturate(lifeProgress) * shockwaveRadiusMax;
            const float ringDistance = abs(distanceFromCenter - shockwaveRadius);
            shapeCoverage = 1.f - smoothstep(shockwaveThickness, shockwaveThickness + shockwaveSoftness, ringDistance);
        }
    }

    return saturate(shapeCoverage);
}

float EffectDistortion_ComputeLensCoverage(float materialCoverage, float shapeCoverage, float presence)
{
    return saturate(materialCoverage * shapeCoverage * max(presence, 0.f));
}

float EffectDistortion_ComputeLensEdgeAlpha(float lensCoverage)
{
    return saturate(lensCoverage);
}

float2 EffectDistortion_ResolveFieldOffset(float2 accumulatedOffset, float accumulatedLensCoverage)
{
    return accumulatedOffset / max(accumulatedLensCoverage, EPSILON);
}

float2 EffectDistortion_ComputeScreenOffset(float2 flow, float intensity)
{
    return flow * intensity;
}

float EffectDistortion_ApplyAxisPolicy(float rawValue, float transformedValue, float policy)
{
    return policy >= 2.5f ? rawValue : transformedValue;
}

float EffectDistortion_ApplyAxisAddress(float value, float policy)
{
    return policy >= 0.5f && policy < 2.5f ? saturate(value) : value;
}

float2 EffectDistortion_BuildAxisSeparatedUV(float2 rawUV, float2 scale, float2 offset, float2 scrollSpeed, float materialTime, float2 policy)
{
    const float2 transformedUV = rawUV * scale + offset + scrollSpeed * materialTime;
    return float2(
        EffectDistortion_ApplyAxisPolicy(rawUV.x, transformedUV.x, policy.x),
        EffectDistortion_ApplyAxisPolicy(rawUV.y, transformedUV.y, policy.y)
    );
}

float2 EffectDistortion_ApplyAxisSeparatedAddress(float2 uv, float2 policy)
{
    return float2(
        EffectDistortion_ApplyAxisAddress(uv.x, policy.x),
        EffectDistortion_ApplyAxisAddress(uv.y, policy.y)
    );
}

float4 EffectDistortion_PackField(float2 offset, float lensCoverage)
{
    const float coverage = saturate(lensCoverage);
    return float4(offset * coverage, coverage, 0.f);
}

#endif
