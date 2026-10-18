// Debug smoke shader for compute-driven instancing.
// C++ binds OutputData as UAV(u0), then copies it into a VERTEX_BUFFER.

struct ParticleInstanceVertex
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

RWStructuredBuffer<ParticleInstanceVertex> OutputData : register(u0);

[numthreads(8, 1, 1)]
void CS_Main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;

    ParticleInstanceVertex instance;
    instance.right = float4(1.0f, 0.0f, 0.0f, 0.0f);
    instance.up = float4(0.0f, 1.0f, 0.0f, 0.0f);
    instance.look = float4(0.0f, 0.0f, 1.0f, 0.0f);
    instance.translation = float4((float)index, (float)index * 0.5f, 0.0f, 1.0f);
    instance.lifeTime = float2(2.0f, (float)index * 0.25f);
    instance.startColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    instance.endColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    instance.subUVRect = float4(0.0f, 0.0f, 1.0f, 1.0f);

    OutputData[index] = instance;
}
