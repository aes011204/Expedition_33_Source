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
    float4 g_SpawnRange;
    float4 g_ScaleLifeTime; // x/y: scale range, z/w: lifetime range
    float4 g_StartColorMin;
    float4 g_StartColorMax;
    float4 g_EndColorMin;
    float4 g_EndColorMax;
    float4 g_SubUVParams;       // x/y: rows/cols
    float4 g_SubUVFrameParams;  // x/y: start/end frame, z: loop, w: enabled
    float4 g_SizeByLifeParams;  // x/y: width start/end, z/w: height start/end
    float4 g_MotionRadialPivot; // xyz: radial pivot, w: motion enabled flag reserved for fallback policy
    float4 g_MotionSpeedDrag;   // x/y: speed min/max, z/w: drag min/max
    float4 g_AccelerationMin;
    float4 g_AccelerationMax;
    float4 g_VelocityScaleByLife;
    float4 g_RotationInitialRate; // x/y: initial rotation min/max, z/w: rate min/max
    float4 g_RotationByLife;      // x/y: rotation start/end, z/w: rate scale start/end
};

RWStructuredBuffer<ParticleInstanceVertex> OutputData : register(u0);

// 정수 seed를 0~1 사이의 pseudo-random float 값으로 바꾼다.
float Hash01(uint seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return (float)(seed & 0x00FFFFFFu) / 16777215.0f;
}

// seed에서 만든 0~1 값으로 min/max 사이의 3축 값을 샘플링한다.
float3 SampleRange3(float3 minValue, float3 maxValue, uint seed)
{
    return lerp(
        minValue,
        maxValue,
        float3(Hash01(seed), Hash01(seed + 17u), Hash01(seed + 31u))
    );
}

// 선형 start/end life 배율을 현재 lifeProgress까지 누적 이동/회전에 쓸 (적분) 평균 배율로 바꾼다.
float IntegrateLifeScale(float scaleStart, float scaleEnd, float lifeProgress)
{
    return scaleStart + 0.5f * (scaleEnd - scaleStart) * lifeProgress;
}

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

    const float ratio = ((float)index + 0.5f) / max(1.0f, g_InstanceCount);
    const float scaleRatio = frac(ratio * 7.0f);
    const float colorRatio = frac(ratio * 11.0f);
    const float baseScale = lerp(g_ScaleLifeTime.x, g_ScaleLifeTime.y, scaleRatio);
    const float lifeMax = lerp(g_ScaleLifeTime.z, g_ScaleLifeTime.w, frac(ratio * 5.0f));
    const float lifeCurrent = frac(g_ElapsedTime + ratio) * max(lifeMax, 0.0001f);
    const float lifeProgress = saturate(lifeCurrent / max(lifeMax, 0.0001f));
    const float sizeX = baseScale * lerp(g_SizeByLifeParams.x, g_SizeByLifeParams.y, lifeProgress);
    const float sizeY = baseScale * lerp(g_SizeByLifeParams.z, g_SizeByLifeParams.w, lifeProgress);

    // particle index에서 만든 deterministic seed다.
    // 같은 particle은 매 프레임 같은 seed를 사용하므로 spawn offset, speed, rotation 랜덤값이 흔들리지 않는다.
    // 뒤의 +101u, +211u 같은 offset은 X/Y/Z나 속성별 샘플이 서로 같은 패턴을 공유하지 않게 분리하기 위한 값이다.
    const uint seed = index * 9781u + 1u;

    const float3 spawnOffset = (float3(Hash01(seed), Hash01(seed + 101u), Hash01(seed + 211u)) * 2.0f - 1.0f) * g_SpawnRange.xyz * 0.5f;
    const float3 spawnPosition = g_Center.xyz + spawnOffset;
    const float3 pivotPosition = g_Center.xyz + g_MotionRadialPivot.xyz;
    float3 velocityDirection = spawnPosition - pivotPosition;

    if (length(velocityDirection) < 0.0001f)
        velocityDirection = normalize(float3(Hash01(seed + 307u) * 2.0f - 1.0f, 0.25f, Hash01(seed + 401u) * 2.0f - 1.0f));
    else
        velocityDirection = normalize(velocityDirection);

    const float speed = lerp(g_MotionSpeedDrag.x, g_MotionSpeedDrag.y, Hash01(seed + 503u));
    const float drag = max(0.0f, lerp(g_MotionSpeedDrag.z, g_MotionSpeedDrag.w, Hash01(seed + 607u)));
    const float velocityScale = IntegrateLifeScale(g_VelocityScaleByLife.x, g_VelocityScaleByLife.y, lifeProgress);
    const float dragTime = drag > 0.0001f ? (1.0f - exp(-drag * lifeCurrent)) / drag : lifeCurrent;
    const float3 acceleration = SampleRange3(g_AccelerationMin.xyz, g_AccelerationMax.xyz, seed + 701u);
    const float3 motionOffset = velocityDirection * speed * velocityScale * dragTime + acceleration * 0.5f * lifeCurrent * lifeCurrent;

    const float initialRotation = radians(lerp(g_RotationInitialRate.x, g_RotationInitialRate.y, Hash01(seed + 809u)));
    const float rotationOverLife = radians(lerp(g_RotationByLife.x, g_RotationByLife.y, lifeProgress));
    const float rotationRate = radians(lerp(g_RotationInitialRate.z, g_RotationInitialRate.w, Hash01(seed + 907u)));
    const float rotationRateScale = IntegrateLifeScale(g_RotationByLife.z, g_RotationByLife.w, lifeProgress);
    const float rotation = initialRotation + rotationOverLife + rotationRate * rotationRateScale * lifeCurrent;
    const float rotationCos = cos(rotation);
    const float rotationSin = sin(rotation);

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
    instance.right = float4(rotationCos * sizeX, rotationSin * sizeX, 0.0f, 0.0f);
    instance.up = float4(-rotationSin * sizeY, rotationCos * sizeY, 0.0f, 0.0f);
    instance.look = float4(0.0f, 0.0f, baseScale, 0.0f);
    instance.translation = float4(spawnPosition + motionOffset, 1.0f);
    instance.lifeTime = float2(max(lifeMax, 0.0001f), lifeCurrent);
    instance.startColor = lerp(g_StartColorMin, g_StartColorMax, colorRatio);
    instance.endColor = lerp(g_EndColorMin, g_EndColorMax, colorRatio);
    instance.subUVRect = float4(u0, v0, u0 + invCols, v0 + invRows);

    OutputData[index] = instance;
}
