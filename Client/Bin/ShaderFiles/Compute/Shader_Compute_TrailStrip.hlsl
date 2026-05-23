struct TrailSamplePayload
{
    float4 baseWorldPosition;
    float4 tipWorldPosition;
    float4 sampleParams; // x: 수명
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
    float4 g_Tint;
};

StructuredBuffer<TrailSamplePayload> HistoryData : register(t0);
RWStructuredBuffer<TrailInstanceVertex> OutputData : register(u0);
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

    const TrailSamplePayload current = HistoryData[index];
    const TrailSamplePayload next = HistoryData[index + 1u];

    const float3 currentCenter = (current.baseWorldPosition.xyz + current.tipWorldPosition.xyz) * 0.5f;
    const float3 nextCenter = (next.baseWorldPosition.xyz + next.tipWorldPosition.xyz) * 0.5f;
    const float3 center = (currentCenter + nextCenter) * 0.5f;
    const float3 bladeAxis = current.tipWorldPosition.xyz - current.baseWorldPosition.xyz;
    const float age = max(0.0f, current.sampleParams.x);
    const float lifeProgress = saturate(age / max(g_SegmentLifetime, 0.0001f));

    TrailInstanceVertex output = (TrailInstanceVertex)0;
    output.right = float4(bladeAxis, 0.0f);
    output.up = float4(0.0f, max(g_Width, 0.001f), 0.0f, 0.0f);
    output.look = float4(0.0f, 0.0f, 1.0f, 0.0f);
    output.translation = float4(center, 1.0f);
    output.lifeTime = float2(max(g_SegmentLifetime, 0.0001f), age);
    output.startColor = g_Tint;
    output.endColor = float4(g_Tint.rgb, 0.0f);
    output.subUVRect = float4(0.0f, lifeProgress * g_UVTiling, 1.0f, (lifeProgress + 1.0f) * g_UVTiling);

    OutputData[index] = output;
}
