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
    float4 g_SizeByLifeParams; // x/y: width start/end, z: enabled, w: reserved
    float4 g_LengthParams;     // x: maxTrailLength, y: tailFadeLength, zw: reserved
};

StructuredBuffer<TrailSamplePayload> HistoryData : register(t0);
RWStructuredBuffer<TrailInstanceVertex> OutputData : register(u0);
RWStructuredBuffer<DrawIndexedInstancedIndirectArgs> ArgsData : register(u1);

void ApplyTrailWidth(inout float3 basePosition, inout float3 tipPosition)
{
    const float3 center = (basePosition + tipPosition) * 0.5f;
    float3 widthDirection = tipPosition - basePosition;
    const float widthLength = length(widthDirection);
    if (widthLength <= 0.0001f)
        return;

    widthDirection /= widthLength;
    const float halfWidth = max(g_Width, 0.0001f) * 0.5f;
    basePosition = center - widthDirection * halfWidth;
    tipPosition = center + widthDirection * halfWidth;
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
    const float sizeByLifeWidthScale = lerp(g_SizeByLifeParams.x, g_SizeByLifeParams.y, lifeProgress);
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
    output.startColor = g_StartColor;
    output.endColor = g_EndColor;
    output.subUVRect = float4(0.0f, currentDistance * g_UVTiling, 1.0f, nextDistance * g_UVTiling);
    output.segmentParams = float4(lifeProgress, widthScale, ageAlpha * lengthAlpha, g_SideFade); // x: 수명 진행률, y: 폭 배율, z: 알파 배율, w: 가장자리 페이드

    OutputData[index] = output;
}
