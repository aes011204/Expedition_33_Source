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

struct BoneMatrixElement
{
    row_major float4x4 BoneMatrix;
};

struct OutputVertex
{
    float3 Position;
    float3 Normal;
    float3 Tangent;
    float3 Binormal;
    float2 Texcoord;
};

StructuredBuffer<SourceVertex> g_SourceVertices : register(t0);
StructuredBuffer<BoneMatrixElement> g_BoneMatricesBuffer : register(t1);
RWStructuredBuffer<OutputVertex> g_OutputVertices : register(u0);

cbuffer ComputeSkinningCB : register(b0)
{
    uint g_VertexCount;
    uint g_Pad0;
    uint g_Pad1;
    uint g_Pad2;
};

[numthreads(64, 1, 1)]
void CS_Main(uint3 dispatchThreadID : SV_DispatchThreadID) // 몇번째 쓰레드인지 알려주는
{
    const uint vertexIndex = dispatchThreadID.x;
    if (vertexIndex >= g_VertexCount)
        return;

    SourceVertex inputVertex = g_SourceVertices[vertexIndex];

    const float weightW =
        1.f -
        (inputVertex.BlendWeight.x +
         inputVertex.BlendWeight.y +
         inputVertex.BlendWeight.z);

    float4x4 boneMatrix =
        g_BoneMatricesBuffer[inputVertex.BlendIndex.x].BoneMatrix * inputVertex.BlendWeight.x +
        g_BoneMatricesBuffer[inputVertex.BlendIndex.y].BoneMatrix * inputVertex.BlendWeight.y +
        g_BoneMatricesBuffer[inputVertex.BlendIndex.z].BoneMatrix * inputVertex.BlendWeight.z +
        g_BoneMatricesBuffer[inputVertex.BlendIndex.w].BoneMatrix * weightW;

    OutputVertex outputVertex;

    outputVertex.Position = mul(float4(inputVertex.Position, 1.f), boneMatrix).xyz;
    outputVertex.Normal = normalize(mul(float4(inputVertex.Normal, 0.f), boneMatrix).xyz);
    outputVertex.Tangent = normalize(mul(float4(inputVertex.Tangent, 0.f), boneMatrix).xyz);
    outputVertex.Binormal = normalize(mul(float4(inputVertex.Binormal, 0.f), boneMatrix).xyz);
    outputVertex.Texcoord = inputVertex.Texcoord;

    g_OutputVertices[vertexIndex] = outputVertex;
}
