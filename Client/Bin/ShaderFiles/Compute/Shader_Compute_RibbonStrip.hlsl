struct RibbonSamplePayload
{
    float4 currentWorldPosition;
    float4 sampleParams; // x: age, y: lifetime, z: distance, w: widthScale
    float4 startColor;
    float4 endColor;
};

struct RibbonInstanceVertex
{
    float4 previousPosition;
    float4 currentPosition;
    float4 nextPosition;
    float4 nextNextPosition;
    float2 lifeTime;
    float4 startColor;
    float4 endColor;
    float4 subUVRect;
    float4 segmentParams;
};

struct DrawIndexedInstancedIndirectArgs
{
    uint indexCountPerInstance;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

cbuffer RibbonComputeParams : register(b0)
{
    uint g_SegmentCount;
    uint g_RenderAxis;
    float g_TilingDistance;
    float g_VisibleLength;
    float4 g_AxisFallback;
};

StructuredBuffer<RibbonSamplePayload> SampleData : register(t0);
RWStructuredBuffer<RibbonInstanceVertex> OutputData : register(u0);
RWStructuredBuffer<DrawIndexedInstancedIndirectArgs> ArgsData : register(u1);

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

    RibbonSamplePayload previous = SampleData[index];
    const bool hasPreviousSample = index > 0u;
    if (hasPreviousSample)
        previous = SampleData[index - 1u];
    const RibbonSamplePayload current = SampleData[index];
    const RibbonSamplePayload next = SampleData[index + 1u];
    RibbonSamplePayload nextNext = next;
    const bool hasNextNextSample = index + 2u <= g_SegmentCount;
    if (hasNextNextSample)
        nextNext = SampleData[index + 2u];
    const float age = max(0.0f, current.sampleParams.x);
    const float lifetime = max(0.0001f, current.sampleParams.y);
    const float lifeProgress = saturate(age / lifetime);
    const float currentDistance = current.sampleParams.z;
    const float nextDistance = next.sampleParams.z;
    const float visibleLength = max(g_VisibleLength, 0.0001f);
    const float currentV = g_TilingDistance > 0.0f ? currentDistance / max(g_TilingDistance, 0.0001f) : currentDistance / visibleLength;
    const float nextV = g_TilingDistance > 0.0f ? nextDistance / max(g_TilingDistance, 0.0001f) : nextDistance / visibleLength;

    RibbonInstanceVertex output = (RibbonInstanceVertex)0;
    output.previousPosition = float4(previous.currentWorldPosition.xyz, hasPreviousSample ? 1.0f : 0.0f);
    output.currentPosition = current.currentWorldPosition;
    output.nextPosition = next.currentWorldPosition;
    output.nextNextPosition = float4(nextNext.currentWorldPosition.xyz, hasNextNextSample ? 1.0f : 0.0f);
    output.lifeTime = float2(lifetime, age);
    output.startColor = current.startColor;
    output.endColor = current.endColor;
    output.subUVRect = float4(0.0f, currentV, 1.0f, nextV);
    output.segmentParams = float4(
        lifeProgress,
        max(current.sampleParams.w, 0.001f),
        max(next.sampleParams.w, 0.001f),
        1.0f - lifeProgress
    );
    OutputData[index] = output;
}
