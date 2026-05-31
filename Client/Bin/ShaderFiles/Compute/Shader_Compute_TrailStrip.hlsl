struct TrailSamplePayload
{
    float4 baseWorldPosition;
    float4 tipWorldPosition;
    float4 sampleParams; // x: 수명, y: head 기준 누적 거리
};

struct TrailInstanceVertex
{
    float4 right;
    float4 up;
    float4 look;
    float4 translation;
    float2 lifeTime;
    float4 startColor;
    float4 endColor;
    float4 subUVRect;
    float4 segmentParams; // x: 수명 진행률, y: 폭 배율, z: 알파 배율, w: 예비
};

struct DrawIndexedInstancedIndirectArgs
{
    uint indexCountPerInstance;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

cbuffer TrailComputeParams : register(b0)
{
    uint g_SegmentCount;
    float g_SegmentLifetime;
    float g_Width;
    float g_UVTiling;
    float g_SideFade;
    float3 g_TrailParamsPadding;
    float4 g_StartColor;
    float4 g_EndColor;
    float4 g_ColorOverLifeCurveParams; // x: color key count, y: alpha key count, z: color enabled, w: alpha enabled
    float4 g_ColorOverLifeColorCurveTimes;
    float4 g_ColorOverLifeColorCurveValuesR;
    float4 g_ColorOverLifeColorCurveValuesG;
    float4 g_ColorOverLifeColorCurveValuesB;
    float4 g_ColorOverLifeColorCurveArriveR;
    float4 g_ColorOverLifeColorCurveArriveG;
    float4 g_ColorOverLifeColorCurveArriveB;
    float4 g_ColorOverLifeColorCurveLeaveR;
    float4 g_ColorOverLifeColorCurveLeaveG;
    float4 g_ColorOverLifeColorCurveLeaveB;
    float4 g_ColorOverLifeColorCurveModes;
    float4 g_ColorOverLifeAlphaCurveTimes;
    float4 g_ColorOverLifeAlphaCurveValues;
    float4 g_ColorOverLifeAlphaCurveArrive;
    float4 g_ColorOverLifeAlphaCurveLeave;
    float4 g_ColorOverLifeAlphaCurveModes;
    float4 g_SizeByLifeParams;       // x/y: width start/end, z: enabled, w: reserved
    float4 g_SizeByLifeCurveTimes;   // scaleOverLife key times
    float4 g_SizeByLifeCurveValuesX; // scaleOverLife X multipliers
    float4 g_SizeByLifeCurveParams;  // x: key count, y: multiplyX, zw: reserved
    float4 g_LengthParams;           // x: maxTrailLength, y: tailFadeLength, zw: reserved
};

StructuredBuffer<TrailSamplePayload> HistoryData : register(t0);
RWStructuredBuffer<TrailInstanceVertex> OutputData : register(u0);
RWStructuredBuffer<DrawIndexedInstancedIndirectArgs> ArgsData : register(u1);

void ApplyTrailWidth(inout float3 basePosition, inout float3 tipPosition)
{
    const float3 center = (basePosition + tipPosition) * 0.5f;
    const float3 halfSpan = (tipPosition - basePosition) * 0.5f;
    if (length(halfSpan) <= 0.0001f)
        return;

    const float widthRatio = max(g_Width, 0.0001f);
    basePosition = center - halfSpan * widthRatio;
    tipPosition = center + halfSpan * widthRatio;
}

float ReadVec4Component(float4 values, uint index)
{
    if (index == 0u)
        return values.x;
    if (index == 1u)
        return values.y;
    if (index == 2u)
        return values.z;
    return values.w;
}

float EvaluateCompactCurve(
    float lifeProgress,
    uint keyCount,
    float4 times,
    float4 values,
    float4 arriveTangents,
    float4 leaveTangents,
    float4 modes,
    float fallbackValue)
{
    keyCount = max(1u, min(4u, keyCount));

    if (keyCount == 1u || lifeProgress <= ReadVec4Component(times, 0u))
        return ReadVec4Component(values, 0u);

    bool found = false;
    float result = fallbackValue;
    [unroll]
    for (uint keyIndex = 1u; keyIndex < 4u; ++keyIndex)
    {
        if (keyIndex >= keyCount || found)
            continue;

        const float rightTime = ReadVec4Component(times, keyIndex);
        if (lifeProgress > rightTime)
            continue;

        const float leftTime = ReadVec4Component(times, keyIndex - 1u);
        const float leftValue = ReadVec4Component(values, keyIndex - 1u);
        const float rightValue = ReadVec4Component(values, keyIndex);
        const float mode = ReadVec4Component(modes, keyIndex - 1u);
        const float width = max(0.0001f, rightTime - leftTime);
        const float ratio = saturate((lifeProgress - leftTime) / width);

        if (mode < 0.5f)
        {
            result = leftValue;
        }
        else if (mode >= 1.5f)
        {
            const float t2 = ratio * ratio;
            const float t3 = t2 * ratio;
            const float leftLeave = ReadVec4Component(leaveTangents, keyIndex - 1u) * width;
            const float rightArrive = ReadVec4Component(arriveTangents, keyIndex) * width;
            result =
                (2.0f * t3 - 3.0f * t2 + 1.0f) * leftValue +
                (t3 - 2.0f * t2 + ratio) * leftLeave +
                (-2.0f * t3 + 3.0f * t2) * rightValue +
                (t3 - t2) * rightArrive;
        }
        else
        {
            result = lerp(leftValue, rightValue, ratio);
        }

        found = true;
    }

    const float lastTime = ReadVec4Component(times, keyCount - 1u);
    if (!found && lifeProgress > lastTime)
        result = ReadVec4Component(values, keyCount - 1u);

    return result;
}

float4 EvaluateColorOverLife(float lifeProgress, float4 fallbackColor)
{
    float4 result = fallbackColor;

    if (g_ColorOverLifeCurveParams.z != 0.0f)
    {
        const uint colorKeyCount = (uint)g_ColorOverLifeCurveParams.x;
        result.rgb = saturate(float3(
            EvaluateCompactCurve(lifeProgress, colorKeyCount, g_ColorOverLifeColorCurveTimes, g_ColorOverLifeColorCurveValuesR, g_ColorOverLifeColorCurveArriveR, g_ColorOverLifeColorCurveLeaveR, g_ColorOverLifeColorCurveModes, fallbackColor.r),
            EvaluateCompactCurve(lifeProgress, colorKeyCount, g_ColorOverLifeColorCurveTimes, g_ColorOverLifeColorCurveValuesG, g_ColorOverLifeColorCurveArriveG, g_ColorOverLifeColorCurveLeaveG, g_ColorOverLifeColorCurveModes, fallbackColor.g),
            EvaluateCompactCurve(lifeProgress, colorKeyCount, g_ColorOverLifeColorCurveTimes, g_ColorOverLifeColorCurveValuesB, g_ColorOverLifeColorCurveArriveB, g_ColorOverLifeColorCurveLeaveB, g_ColorOverLifeColorCurveModes, fallbackColor.b)
        ));
    }

    if (g_ColorOverLifeCurveParams.w != 0.0f)
    {
        const uint alphaKeyCount = (uint)g_ColorOverLifeCurveParams.y;
        result.a = saturate(EvaluateCompactCurve(
            lifeProgress,
            alphaKeyCount,
            g_ColorOverLifeAlphaCurveTimes,
            g_ColorOverLifeAlphaCurveValues,
            g_ColorOverLifeAlphaCurveArrive,
            g_ColorOverLifeAlphaCurveLeave,
            g_ColorOverLifeAlphaCurveModes,
            fallbackColor.a
        ));
    }

    return result;
}

float EvaluateSizeByLifeWidthScale(float lifeProgress)
{
    if (g_SizeByLifeCurveParams.y == 0.0f)
        return 1.0f;

    const uint keyCount = max(1u, min(4u, (uint)g_SizeByLifeCurveParams.x));
    float multiplier = g_SizeByLifeParams.x;

    if (keyCount == 1u || lifeProgress <= ReadVec4Component(g_SizeByLifeCurveTimes, 0u))
    {
        multiplier = ReadVec4Component(g_SizeByLifeCurveValuesX, 0u);
    }
    else
    {
        bool found = false;
        [unroll]
        for (uint keyIndex = 1u; keyIndex < 4u; ++keyIndex)
        {
            if (keyIndex >= keyCount || found)
                continue;

            const float rightTime = ReadVec4Component(g_SizeByLifeCurveTimes, keyIndex);
            if (lifeProgress > rightTime)
                continue;

            const float leftTime = ReadVec4Component(g_SizeByLifeCurveTimes, keyIndex - 1u);
            const float ratio = saturate((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime));
            multiplier = lerp(
                ReadVec4Component(g_SizeByLifeCurveValuesX, keyIndex - 1u),
                ReadVec4Component(g_SizeByLifeCurveValuesX, keyIndex),
                ratio
            );
            found = true;
        }

        const float lastTime = ReadVec4Component(g_SizeByLifeCurveTimes, keyCount - 1u);
        if (!found && lifeProgress > lastTime)
            multiplier = ReadVec4Component(g_SizeByLifeCurveValuesX, keyCount - 1u);
    }

    return multiplier;
}

[numthreads(64, 1, 1)]
void CS_Main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;

    if (index == 0u)
    {
        ArgsData[0].indexCountPerInstance = 1u;
        ArgsData[0].instanceCount = g_SegmentCount;
        ArgsData[0].startIndexLocation = 0u;
        ArgsData[0].baseVertexLocation = 0;
        ArgsData[0].startInstanceLocation = 0u;
    }

    if (index >= g_SegmentCount)
        return;

    const TrailSamplePayload current = HistoryData[index];
    const TrailSamplePayload next = HistoryData[index + 1u];
    float3 currentBase = current.baseWorldPosition.xyz;
    float3 currentTip = current.tipWorldPosition.xyz;
    float3 nextBase = next.baseWorldPosition.xyz;
    float3 nextTip = next.tipWorldPosition.xyz;
    ApplyTrailWidth(currentBase, currentTip);
    ApplyTrailWidth(nextBase, nextTip);

    const float age = max(0.0f, current.sampleParams.x);
    const float lifeProgress = saturate(age / max(g_SegmentLifetime, 0.0001f));
    const float ageAlpha = saturate(1.0f - lifeProgress);
    const float fallbackWidthScale = lerp(0.25f, 1.0f, ageAlpha);
    const float sizeByLifeWidthScale = EvaluateSizeByLifeWidthScale(lifeProgress);
    const float widthScale = g_SizeByLifeParams.z != 0.0f ? sizeByLifeWidthScale : fallbackWidthScale;
    const float currentDistance = current.sampleParams.y;
    const float nextDistance = next.sampleParams.y;
    float lengthAlpha = 1.0f;
    if (g_LengthParams.x > 0.0f)
    {
        const float fadeLength = max(g_LengthParams.y, 0.001f);
        const float fadeStart = max(0.0f, g_LengthParams.x - fadeLength);
        lengthAlpha = saturate((g_LengthParams.x - currentDistance) / max(g_LengthParams.x - fadeStart, 0.001f));
    }

    TrailInstanceVertex output = (TrailInstanceVertex)0;
    output.right = float4(currentBase, 1.f);
    output.up = float4(currentTip, 1.f);
    output.look = float4(nextBase, 1.f);
    output.translation = float4(nextTip, 1.f);
    output.lifeTime = float2(max(g_SegmentLifetime, 0.0001f), age);
    const float4 segmentColor = EvaluateColorOverLife(lifeProgress, lerp(g_StartColor, g_EndColor, lifeProgress));
    output.startColor = segmentColor;
    output.endColor = segmentColor;
    output.subUVRect = float4(0.0f, currentDistance, 1.0f, nextDistance);
    output.segmentParams = float4(lifeProgress, widthScale, ageAlpha * lengthAlpha, g_SideFade); // x: 수명 진행률, y: 폭 배율, z: 알파 배율, w: 가장자리 페이드

    OutputData[index] = output;
}
