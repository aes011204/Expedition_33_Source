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
    float4 g_SubUVParams;      // x/y: rows/cols
    float4 g_SubUVFrameParams; // x/y: start/end frame, z: loop, w: enabled
    float4 g_SizeByLifeParams; // x/y: width start/end, z/w: height start/end
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
    const float baseScale = lerp(g_ScaleLifeTime.x, g_ScaleLifeTime.y, scaleRatio);
    const float lifeMax = lerp(g_ScaleLifeTime.z, g_ScaleLifeTime.w, frac(ratio * 5.0f));
    const float radius = 1.2f + frac(ratio * 5.0f) * 1.0f;
    const float lifeCurrent = frac(g_ElapsedTime + ratio) * max(lifeMax, 0.0001f);
    const float lifeProgress = saturate(lifeCurrent / max(lifeMax, 0.0001f));
    const float sizeX = baseScale * lerp(g_SizeByLifeParams.x, g_SizeByLifeParams.y, lifeProgress);
    const float sizeY = baseScale * lerp(g_SizeByLifeParams.z, g_SizeByLifeParams.w, lifeProgress);

    const uint rows = max(1u, (uint)g_SubUVParams.x);
    const uint cols = max(1u, (uint)g_SubUVParams.y);
    const uint frameCount = max(1u, rows * cols);
    uint frameIndex = 0u;

    if (g_SubUVFrameParams.w != 0.0f)
    {
        const uint lastFrame = frameCount - 1u;
        const uint startFrame = min((uint)g_SubUVFrameParams.x, lastFrame);
        uint endFrame = min((uint)g_SubUVFrameParams.y, lastFrame);

        if ((uint)g_SubUVFrameParams.x == 0u && (uint)g_SubUVFrameParams.y == 0u && frameCount > 1u)
            endFrame = lastFrame;

        if (endFrame < startFrame)
            endFrame = startFrame;

        const uint span = endFrame - startFrame;
        frameIndex = startFrame + (uint)round((float)span * lifeProgress);
    }

    frameIndex = min(frameIndex, frameCount - 1u);

    const uint row = frameIndex / cols;
    const uint col = frameIndex % cols;
    const float invCols = 1.0f / (float)cols;
    const float invRows = 1.0f / (float)rows;
    const float u0 = (float)col * invCols;
    const float v0 = (float)row * invRows;

    ParticleInstanceVertex instance;
    instance.right = float4(sizeX, 0.0f, 0.0f, 0.0f);
    instance.up = float4(0.0f, sizeY, 0.0f, 0.0f);
    instance.look = float4(0.0f, 0.0f, baseScale, 0.0f);
    instance.translation = float4(g_Center.x + cos(angle) * radius, g_Center.y + sin(angle * 1.7f) * 0.5f, g_Center.z + sin(angle) * radius, 1.0f);
    instance.lifeTime = float2(max(lifeMax, 0.0001f), lifeCurrent);
    instance.startColor = lerp(g_StartColorMin, g_StartColorMax, colorRatio);
    instance.endColor = lerp(g_EndColorMin, g_EndColorMax, colorRatio);
    instance.subUVRect = float4(u0, v0, u0 + invCols, v0 + invRows);

    OutputData[index] = instance;
}
