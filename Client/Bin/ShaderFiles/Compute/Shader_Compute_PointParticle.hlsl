// Debug smoke shader for compute-driven instancing.
// C++ binds OutputData as UAV(u0), then copies it into a VERTEX_BUFFER.

struct ParticleInstanceVertex
{
    float4 right;
    float4 up;
    float4 look;
    float4 translation;
    float2 lifeTime;
};

cbuffer ComputePointParticleParams : register(b0)
{
    float g_ElapsedTime;
    float g_InstanceCount;
    float2 g_Padding;
};

RWStructuredBuffer<ParticleInstanceVertex> OutputData : register(u0);

[numthreads(64, 1, 1)]
void CS_Main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= (uint)g_InstanceCount)
        return;

    const float ratio = ((float)index + 0.5f) / max(g_InstanceCount, 1.0f);
    const float angle = ratio * 6.2831853f + g_ElapsedTime;
    const float scale = 0.18f + frac(ratio * 7.0f) * 0.12f;
    const float radius = 1.2f + frac(ratio * 5.0f) * 1.0f;

    ParticleInstanceVertex instance;
    instance.right = float4(scale, 0.0f, 0.0f, 0.0f);
    instance.up = float4(0.0f, scale, 0.0f, 0.0f);
    instance.look = float4(0.0f, 0.0f, scale, 0.0f);
    instance.translation = float4(cos(angle) * radius, 2.0f + sin(angle * 1.7f) * 0.5f, sin(angle) * radius, 1.0f);
    instance.lifeTime = float2(2.0f, frac(g_ElapsedTime + ratio));

    OutputData[index] = instance;
}
