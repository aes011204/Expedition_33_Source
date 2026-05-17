// Debug smoke shader for compute-driven instancing.
// C++ binds OutputData as UAV(u0), then copies it into a VERTEX_BUFFER.
// LifecycleState(u2)는 particle별 age/lifetime을 frame 사이에 유지한다.

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

struct ParticleLifecycleState
{
    float lifeAge;
    float lifeMax;
    uint respawnSeed;
    uint padding;
};

cbuffer ComputePointParticleParams : register(b0)
{
    float g_ElapsedTime;
    float g_DeltaTime;
    float g_InstanceCount;
    uint g_DrawMode;
    float4 g_Center;
    float4 g_SpawnRange;
    float4 g_SizeMin;               // x/y: width/height min
    float4 g_SizeMax;               // x/y: width/height max
    float4 g_LifeTimeParams;        // x/y: lifetime min/max
    float4 g_StartColorMin;
    float4 g_StartColorMax;
    float4 g_EndColorMin;
    float4 g_EndColorMax;
    float4 g_SubUVParams;           // x/y: rows/cols, z: playback mode, w: frames per second
    float4 g_SubUVFrameParams;      // x/y: start/end frame, z: loop, w: enabled
    float4 g_SubUVFrameCurveTimes;  // Reserved frame curve key times for later Curve Editor work
    float4 g_SubUVFrameCurveValues; // Reserved frame curve frame values for later Curve Editor work
    float4 g_SubUVFrameCurveParams; // x: key count
    float4 g_SizeByLifeParams;      // x/y: width start/end, z/w: height start/end
    float4 g_MotionRadialPivot;     // xyz: radial pivot, w: motion enabled flag reserved for fallback policy
    float4 g_MotionSpeedDrag;       // x/y: speed min/max, z/w: drag min/max
    float4 g_AccelerationMin;
    float4 g_AccelerationMax;
    float4 g_VelocityScaleByLife;
    float4 g_RotationInitialRate; // x/y: initial rotation min/max, z/w: rate min/max
    float4 g_RotationByLife;      // x/y: rotation start/end, z/w: rate scale start/end
};

RWStructuredBuffer<ParticleInstanceVertex> OutputData : register(u0);
RWStructuredBuffer<ParticleLifecycleState> LifecycleState : register(u2);

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

float SampleLifeMax(uint seed)
{
    const float lifeMin = max(0.0001f, min(g_LifeTimeParams.x, g_LifeTimeParams.y));
    const float lifeMax = max(lifeMin, max(g_LifeTimeParams.x, g_LifeTimeParams.y));
    return lerp(lifeMin, lifeMax, Hash01(seed + 43u));
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

    if (index == 0 && g_DrawMode != 0)
    {
        ComputeIndirectArgs args;
        args.value0 = 1u;
        args.instanceCount = maxInstanceCount;
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
    const float2 baseSize = lerp(g_SizeMin.xy, g_SizeMax.xy, scaleRatio);

    ParticleLifecycleState lifecycle = LifecycleState[index];
    lifecycle.lifeMax = max(lifecycle.lifeMax, 0.0001f);
    lifecycle.lifeAge += max(0.0f, g_DeltaTime);

    if (lifecycle.lifeAge >= lifecycle.lifeMax)
    {
        lifecycle.respawnSeed += 4093u;
        lifecycle.lifeMax = SampleLifeMax(lifecycle.respawnSeed);
        lifecycle.lifeAge = 0.0f;
    }

    const float lifeMax = max(lifecycle.lifeMax, 0.0001f);
    const float lifeAge = max(0.0f, lifecycle.lifeAge);
    const float lifeProgress = saturate(lifeAge / lifeMax);
    const float sizeX = baseSize.x * lerp(g_SizeByLifeParams.x, g_SizeByLifeParams.y, lifeProgress);
    const float sizeY = baseSize.y * lerp(g_SizeByLifeParams.z, g_SizeByLifeParams.w, lifeProgress);

    // particle index에서 만든 deterministic seed다.
    // 같은 particle 생애 안에서는 같은 seed를 사용하므로 spawn offset, speed, rotation 랜덤값이 흔들리지 않는다.
    // 뒤의 +101u, +211u 같은 offset은 X/Y/Z나 속성별 샘플이 서로 같은 패턴을 공유하지 않게 분리하기 위한 값이다.
    const uint seed = lifecycle.respawnSeed;

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
    const float dragTime = drag > 0.0001f ? (1.0f - exp(-drag * lifeAge)) / drag : lifeAge;
    const float3 acceleration = SampleRange3(g_AccelerationMin.xyz, g_AccelerationMax.xyz, seed + 701u);
    const float3 motionOffset = velocityDirection * speed * velocityScale * dragTime + acceleration * 0.5f * lifeAge * lifeAge;

    const float initialRotation = radians(lerp(g_RotationInitialRate.x, g_RotationInitialRate.y, Hash01(seed + 809u)));
    const float rotationOverLife = radians(lerp(g_RotationByLife.x, g_RotationByLife.y, lifeProgress));
    const float rotationRate = radians(lerp(g_RotationInitialRate.z, g_RotationInitialRate.w, Hash01(seed + 907u)));
    const float rotationRateScale = IntegrateLifeScale(g_RotationByLife.z, g_RotationByLife.w, lifeProgress);
    const float rotation = initialRotation + rotationOverLife + rotationRate * rotationRateScale * lifeAge;
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

        if (endFrame < startFrame)
            endFrame = startFrame;

        const uint span = endFrame - startFrame;
        const uint rangeCount = span + 1u;

        if ((uint)g_SubUVParams.z == 0u)
        {
            frameIndex = startFrame;
        }
        else if ((uint)g_SubUVParams.z == 1u)
        {
            const uint frameOffset = min(
                (uint)floor(saturate(lifeProgress) * (float)rangeCount),
                rangeCount - 1u
            );
            frameIndex = startFrame + frameOffset;
        }
        else if ((uint)g_SubUVParams.z == 2u)
        {
            const uint frameOffset = (uint)floor(lifeAge * max(0.0f, g_SubUVParams.w));
            if (g_SubUVFrameParams.z != 0.0f && rangeCount > 0u)
                frameIndex = startFrame + frameOffset % rangeCount;
            else
                frameIndex = startFrame + min(frameOffset, span);
        }
        else if ((uint)g_SubUVParams.z == 3u)
        {
            const uint frameOffset = min(
                (uint)floor(Hash01(seed + 1201u) * (float)rangeCount),
                rangeCount - 1u
            );
            frameIndex = startFrame + frameOffset;
        }
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
    instance.look = float4(0.0f, 0.0f, max(baseSize.x, baseSize.y), 0.0f);
    instance.translation = float4(spawnPosition + motionOffset, 1.0f);
    instance.lifeTime = float2(lifeMax, lifeAge);
    instance.startColor = lerp(g_StartColorMin, g_StartColorMax, colorRatio);
    instance.endColor = lerp(g_EndColorMin, g_EndColorMax, colorRatio);
    instance.subUVRect = float4(u0, v0, u0 + invCols, v0 + invRows);

    OutputData[index] = instance;
    LifecycleState[index] = lifecycle;
}
