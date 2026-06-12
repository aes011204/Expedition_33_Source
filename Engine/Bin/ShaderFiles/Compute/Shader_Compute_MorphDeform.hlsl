#include "../Engine_Shader_ComputeAnimTypes.hlsli"

struct SourceVertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
    float3 Binormal;
    float2 Texcoord;
    uint4 BlendIndex;
    float4 BlendWeight;
};

StructuredBuffer<SourceVertex> g_SourceVertices : register(t0);
StructuredBuffer<MorphVertexRange> g_VertexRanges : register(t1);
StructuredBuffer<MorphGpuInfluence> g_Influences : register(t2);
StructuredBuffer<float> g_MorphWeights : register(t3);
RWStructuredBuffer<SourceVertex> g_OutputVertices : register(u0);

cbuffer MorphDeformCB : register(b0)
{
    uint g_VertexCount;
    uint g_TargetCount;
    uint g_Pad0;
    uint g_Pad1;
};

[numthreads(64, 1, 1)]
void CS_Main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const uint vertexIndex = dispatchThreadID.x;
    if (vertexIndex >= g_VertexCount)
        return;

    SourceVertex outputVertex = g_SourceVertices[vertexIndex];
    const MorphVertexRange range = g_VertexRanges[vertexIndex];

    float3 position = outputVertex.Position;
    float3 normal = outputVertex.Normal;

    for (uint i = 0; i < range.Count; ++i)
    {
        const MorphGpuInfluence influence = g_Influences[range.Start + i];
        if (influence.TargetIndex >= g_TargetCount)
            continue;

        const float weight = g_MorphWeights[influence.TargetIndex] * influence.WeightScale;
        position += influence.DeltaPosition * weight;
        normal += influence.DeltaNormal * weight;
    }

    outputVertex.Position = position;
    outputVertex.Normal = normalize(normal);

    g_OutputVertices[vertexIndex] = outputVertex;
}
