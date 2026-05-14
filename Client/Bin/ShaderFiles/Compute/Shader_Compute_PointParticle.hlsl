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
};

cbuffer ComputePointParticleParams : register(b0)
{
    float g_ElapsedTime;
    float g_InstanceCount;
    uint g_DrawMode;
    float g_Padding;
    float4 g_Center;
    float4 g_ScaleLifeTime; // x/y: scale range, z/w: lifetime range
    float4 g_StartColorMin;
    float4 g_StartColorMax;
    float4 g_EndColorMin;
    float4 g_EndColorMax;
};

RWStructuredBuffer<ParticleInstanceVertex> OutputData : register(u0);

struct ComputeIndirectArgs
{
    uint value0;
    uint instanceCount;
    uint value2;
    int value3;
    uint startInstanceLocation;
};

RWStructuredBuffer<ComputeIndirectArgs> ArgsData : register(u1);

[numthreads(64, 1, 1)]
void CS_Main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    const uint maxInstanceCount = (uint)g_InstanceCount;
    const uint minInstanceCount = min(16u, maxInstanceCount);
    const float visibleRatio = 0.5f + 0.5f * sin(g_ElapsedTime * 2.0f);
    const uint visibleCount = max(minInstanceCount, (uint)(visibleRatio * (float)maxInstanceCount));

    if (index == 0 && g_DrawMode != 0)
    {
        ComputeIndirectArgs args;
        args.value0 = 1u;
        args.instanceCount = min(max(visibleCount, 1u), maxInstanceCount);
        args.value2 = 0u;
        args.value3 = 0;
        args.startInstanceLocation = 0u;

        ArgsData[0] = args;
    }

    if (index >= (uint)g_InstanceCount)
        return;

    const float ratio = ((float)index + 0.5f) / max(g_InstanceCount, 1.0f);
    const float angle = ratio * 6.2831853f + g_ElapsedTime;
    const float scaleRatio = frac(ratio * 7.0f);
    const float colorRatio = frac(ratio * 11.0f);
    const float scale = lerp(g_ScaleLifeTime.x, g_ScaleLifeTime.y, scaleRatio);
    const float lifeMax = lerp(g_ScaleLifeTime.z, g_ScaleLifeTime.w, frac(ratio * 5.0f));
    const float radius = 1.2f + frac(ratio * 5.0f) * 1.0f;

    ParticleInstanceVertex instance;
    instance.right = float4(scale, 0.0f, 0.0f, 0.0f);
    instance.up = float4(0.0f, scale, 0.0f, 0.0f);
    instance.look = float4(0.0f, 0.0f, scale, 0.0f);
    instance.translation = float4(g_Center.x + cos(angle) * radius, g_Center.y + sin(angle * 1.7f) * 0.5f, g_Center.z + sin(angle) * radius, 1.0f);
    instance.lifeTime = float2(max(lifeMax, 0.0001f), frac(g_ElapsedTime + ratio) * max(lifeMax, 0.0001f));
    instance.startColor = lerp(g_StartColorMin, g_StartColorMax, colorRatio);
    instance.endColor = lerp(g_EndColorMin, g_EndColorMax, colorRatio);

    OutputData[index] = instance;
}
