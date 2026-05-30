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
    uint active;
};

cbuffer ComputePointParticleParams : register(b0)
{
    float g_ElapsedTime;
    float g_DeltaTime;
    float g_InstanceCount;
    uint g_DrawMode;
    uint g_SpawnRequest;
    uint g_MaxActiveCount;
    uint g_MaxDrawCount;
    uint g_KillActiveParticles;
    float4 g_Center;
    float4 g_EmitterBasisRight; // xyz: emitter local X axis in world space
    float4 g_EmitterBasisUp;    // xyz: emitter local Y axis in world space
    float4 g_EmitterBasisLook;  // xyz: emitter local Z axis in world space
    float4 g_SpawnRange;
    float4 g_InitialLocationMin;         // xyz: InitialLocation min offset, w: enabled
    float4 g_InitialLocationMax;         // xyz: InitialLocation max offset
    float4 g_SphereLocationOffsetRadius; // xyz: sphere center offset, w: radius
    float4 g_SphereLocationParams;       // x: enabled, y: mode(0 volume, 1 surface)
    float4 g_SizeMin;                    // x/y: width/height min
    float4 g_SizeMax;                    // x/y: width/height max
    float4 g_LifeTimeParams;             // x/y: lifetime min/max
    float4 g_StartColorMin;
    float4 g_StartColorMax;
    float4 g_EndColorMin;
    float4 g_EndColorMax;
    float4 g_SubUVParams;            // x/y: rows/cols, z: playback mode, w: frames per second
    float4 g_SubUVFrameParams;       // x/y: start/end frame, z: loop, w: enabled
    float4 g_SubUVFrameCurveTimes;   // Reserved frame curve key times for later Curve Editor work
    float4 g_SubUVFrameCurveValues;  // Reserved frame curve frame values for later Curve Editor work
    float4 g_SubUVFrameCurveParams;  // x: key count
    float4 g_SizeByLifeParams;       // x/y: width start/end, z/w: height start/end
    float4 g_SizeByLifeCurveTimes;   // scaleOverLife key times
    float4 g_SizeByLifeCurveValuesX; // scaleOverLife X multipliers
    float4 g_SizeByLifeCurveValuesY; // scaleOverLife Y multipliers
    float4 g_SizeByLifeCurveParams;  // x: key count, y: multiplyX, z: multiplyY, w: axisLock
    float4 g_MotionFlags;            // x: motion enabled, y: vector enabled, z: vector world, w: radial enabled
    float4 g_MotionSpaceFlags;       // x: radial world
    float4 g_InitialVelocityMin;     // xyz: vector velocity min
    float4 g_InitialVelocityMax;     // xyz: vector velocity max
    float4 g_MotionRadialPivot;      // xyz: radial pivot
    float4 g_MotionRadialSpeedDrag;  // x/y: radial speed min/max, z/w: drag min/max
    float4 g_AccelerationMin;
    float4 g_AccelerationMax;
    float4 g_VelocityScaleByLife;
    float4 g_RotationInitialRate;         // x/y: initial rotation min/max, z/w: rate min/max
    float4 g_RotationByLife;              // (Legacy) x/y: rotation start/end, z/w: rate scale start/end
    float4 g_RotationOverLifeCurveTimes;  // rotationOverLife key times
    float4 g_RotationOverLifeCurveValues; // rotationOverLife degree values
    float4 g_RotationOverLifeCurveParams; // x: key count
};

RWStructuredBuffer<ParticleInstanceVertex> OutputData : register(u0);
RWStructuredBuffer<ParticleLifecycleState> LifecycleState : register(u2);

float ReadVec4Component(float4 values, uint index)
{
    if (index == 0u)
        return values.x;
    if (index == 1u)
        return values.y;
    if (index == 2u)
        return values.z;
    return values.w;
}

float2 EvaluateSizeByLifeMultiplier(float lifeProgress)
{
    const uint keyCount = max(1u, min(4u, (uint)g_SizeByLifeCurveParams.x));
    float2 multiplier = float2(g_SizeByLifeParams.x, g_SizeByLifeParams.z);

    if (keyCount == 1u || lifeProgress <= ReadVec4Component(g_SizeByLifeCurveTimes, 0u))
    {
        multiplier = float2(
            ReadVec4Component(g_SizeByLifeCurveValuesX, 0u),
            ReadVec4Component(g_SizeByLifeCurveValuesY, 0u)
        );
    }
    else
    {
        bool found = false;
        [unroll]
        for (uint keyIndex = 1u; keyIndex < 4u; ++keyIndex)
        {
            if (keyIndex >= keyCount || found)
                continue;

            const float rightTime = ReadVec4Component(g_SizeByLifeCurveTimes, keyIndex);
            if (lifeProgress > rightTime)
                continue;

            const float leftTime = ReadVec4Component(g_SizeByLifeCurveTimes, keyIndex - 1u);
            const float ratio = saturate((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime));
            multiplier = float2(
                lerp(
                    ReadVec4Component(g_SizeByLifeCurveValuesX, keyIndex - 1u),
                    ReadVec4Component(g_SizeByLifeCurveValuesX, keyIndex),
                    ratio
                ),
                lerp(
                    ReadVec4Component(g_SizeByLifeCurveValuesY, keyIndex - 1u),
                    ReadVec4Component(g_SizeByLifeCurveValuesY, keyIndex),
                    ratio
                )
            );
            found = true;
        }

        const float lastTime = ReadVec4Component(g_SizeByLifeCurveTimes, keyCount - 1u);
        if (!found && lifeProgress > lastTime)
        {
            multiplier = float2(
                ReadVec4Component(g_SizeByLifeCurveValuesX, keyCount - 1u),
                ReadVec4Component(g_SizeByLifeCurveValuesY, keyCount - 1u)
            );
        }
    }

    if ((uint)g_SizeByLifeCurveParams.w == 1u)
        multiplier.y = multiplier.x;
    else if ((uint)g_SizeByLifeCurveParams.w == 2u)
        multiplier.x = multiplier.y;

    if (g_SizeByLifeCurveParams.y == 0.0f)
        multiplier.x = 1.0f;
    if (g_SizeByLifeCurveParams.z == 0.0f)
        multiplier.y = 1.0f;

    return multiplier;
}

// RotationOverLife curve payload를 평가해 life-progress 시점의 추가 회전각을 반환한다.
float EvaluateRotationOverLifeDegrees(float lifeProgress)
{
    const uint keyCount = max(1u, min(4u, (uint)g_RotationOverLifeCurveParams.x));
    float rotationDegrees = g_RotationByLife.x;

    if (keyCount == 1u || lifeProgress <= ReadVec4Component(g_RotationOverLifeCurveTimes, 0u))
        rotationDegrees = ReadVec4Component(g_RotationOverLifeCurveValues, 0u);
    else
    {
        bool found = false;
        [unroll]
        for (uint keyIndex = 1u; keyIndex < 4u; ++keyIndex)
        {
            if (keyIndex >= keyCount || found)
                continue;

            const float rightTime = ReadVec4Component(g_RotationOverLifeCurveTimes, keyIndex);
            if (lifeProgress > rightTime)
                continue;

            const float leftTime = ReadVec4Component(g_RotationOverLifeCurveTimes, keyIndex - 1u);
            const float ratio = saturate((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime));
            rotationDegrees = lerp(
                ReadVec4Component(g_RotationOverLifeCurveValues, keyIndex - 1u),
                ReadVec4Component(g_RotationOverLifeCurveValues, keyIndex),
                ratio
            );
            found = true;
        }

        const float lastTime = ReadVec4Component(g_RotationOverLifeCurveTimes, keyCount - 1u);
        if (!found && lifeProgress > lastTime)
            rotationDegrees = ReadVec4Component(g_RotationOverLifeCurveValues, keyCount - 1u);
    }

    return rotationDegrees;
}

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

// emitter local 축 기준 값을 월드 방향 오프셋으로 변환한다.
float3 ResolveEmitterBasisVector(float3 localValue)
{
    const float rightLength = length(g_EmitterBasisRight.xyz);
    const float upLength = length(g_EmitterBasisUp.xyz);
    const float lookLength = length(g_EmitterBasisLook.xyz);
    const float3 right = rightLength < 0.0001f ? float3(1.0f, 0.0f, 0.0f) : g_EmitterBasisRight.xyz / rightLength;
    const float3 up = upLength < 0.0001f ? float3(0.0f, 1.0f, 0.0f) : g_EmitterBasisUp.xyz / upLength;
    const float3 look = lookLength < 0.0001f ? float3(0.0f, 0.0f, 1.0f) : g_EmitterBasisLook.xyz / lookLength;
    return right * localValue.x + up * localValue.y + look * localValue.z;
}

// sphere location 설정에 따라 구 내부 spawn 오프셋을 샘플링한다.
float3 SampleSphereLocationOffset(uint seed)
{
    const float radius = max(0.0f, g_SphereLocationOffsetRadius.w);
    const float twoPi = 6.28318530718f;
    const float z = Hash01(seed + 1301u) * 2.0f - 1.0f;
    const float angle = Hash01(seed + 1409u) * twoPi;
    const float xy = sqrt(max(0.0f, 1.0f - z * z));
    const float3 direction = float3(cos(angle) * xy, sin(angle) * xy, z);
    const float distanceScale = (uint)g_SphereLocationParams.y == 1u
                                ? 1.0f
                                : pow(Hash01(seed + 1511u), 1.0f / 3.0f);
    const float enabled = g_SphereLocationParams.x != 0.0f && radius > 0.0001f ? 1.0f : 0.0f;

    return ResolveEmitterBasisVector(g_SphereLocationOffsetRadius.xyz + direction * radius * distanceScale) * enabled;
}

// life에 따라 선형 변화하는 배율을 누적 이동/회전에 쓸 평균값으로 바꾼다.
float IntegrateLifeScale(float scaleStart, float scaleEnd, float lifeProgress)
{
    return scaleStart + 0.5f * (scaleEnd - scaleStart) * lifeProgress;
}

// 수명 min/max 범위에서 이번 particle의 최대 수명을 샘플링한다.
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
    const uint maxActiveCount = min(maxInstanceCount, max(1u, g_MaxActiveCount));
    const uint maxDrawCount = min(maxInstanceCount, max(1u, g_MaxDrawCount));
    const uint stableDrawCount = min(maxActiveCount, maxDrawCount);

    if (index >= (uint)g_InstanceCount)
        return;

    if (index == 0u)
        ArgsData[0].instanceCount = stableDrawCount;

    ParticleLifecycleState lifecycle = LifecycleState[index];

    if (g_KillActiveParticles != 0u)
    {
        lifecycle.lifeAge = 0.0f;
        lifecycle.active = 0u;
        LifecycleState[index] = lifecycle;
        return;
    }

    if (index >= stableDrawCount)
    {
        lifecycle.active = 0u;
        LifecycleState[index] = lifecycle;
        return;
    }

    lifecycle.lifeMax = max(lifecycle.lifeMax, 0.0001f);

    if (lifecycle.active != 0u)
    {
        lifecycle.lifeAge += max(0.0f, g_DeltaTime);
        if (lifecycle.lifeAge >= lifecycle.lifeMax)
            lifecycle.active = 0u;
    }

    if (lifecycle.active == 0u && g_SpawnRequest > 0u)
    {
        uint spawnCandidate = 0u;
        InterlockedAdd(ArgsData[0].value2, 1u, spawnCandidate);

        if (spawnCandidate < g_SpawnRequest)
        {
            lifecycle.respawnSeed += 4093u;
            lifecycle.lifeMax = SampleLifeMax(lifecycle.respawnSeed);
            lifecycle.lifeAge = 0.0f;
            lifecycle.active = 1u;
        }
    }

    if (lifecycle.active == 0u)
    {
        lifecycle.lifeAge = 0.0f;
        if (index < stableDrawCount)
            OutputData[index] = (ParticleInstanceVertex)0;
        LifecycleState[index] = lifecycle;
        return;
    }

    const float ratio = ((float)index + 0.5f) / max(1.0f, g_InstanceCount);
    const float scaleRatio = frac(ratio * 7.0f);
    const float colorRatio = frac(ratio * 11.0f);
    const float2 baseSize = lerp(g_SizeMin.xy, g_SizeMax.xy, scaleRatio);

    const float lifeMax = max(lifecycle.lifeMax, 0.0001f);
    const float lifeAge = max(0.0f, lifecycle.lifeAge);
    const float lifeProgress = saturate(lifeAge / lifeMax);
    const float2 sizeByLifeMultiplier = EvaluateSizeByLifeMultiplier(lifeProgress);
    const float sizeX = baseSize.x * sizeByLifeMultiplier.x;
    const float sizeY = baseSize.y * sizeByLifeMultiplier.y;

    // particle index에서 만든 deterministic seed다.
    // 같은 particle 생애 안에서는 같은 seed를 사용하므로 spawn offset, speed, rotation 랜덤값이 흔들리지 않는다.
    // 뒤의 +101u, +211u 같은 offset은 X/Y/Z나 속성별 샘플이 서로 같은 패턴을 공유하지 않게 분리하기 위한 값이다.
    const uint seed = lifecycle.respawnSeed;

    const float3 locationRandom = float3(Hash01(seed), Hash01(seed + 101u), Hash01(seed + 211u));
    const float3 fallbackSpawnOffset = ResolveEmitterBasisVector((locationRandom * 2.0f - 1.0f) * g_SpawnRange.xyz * 0.5f);
    const float3 initialLocationOffset = ResolveEmitterBasisVector(lerp(g_InitialLocationMin.xyz, g_InitialLocationMax.xyz, locationRandom));
    const float3 baseLocationOffset = g_InitialLocationMin.w != 0.0f
                                      ? initialLocationOffset
                                      : g_SphereLocationParams.x != 0.0f ? float3(0.0f, 0.0f, 0.0f) : fallbackSpawnOffset;
    const float3 spawnOffset = baseLocationOffset + SampleSphereLocationOffset(seed);
    const float3 spawnPosition = g_Center.xyz + spawnOffset;
    float3 initialVelocity = float3(0.0f, 0.0f, 0.0f);

    if (g_MotionFlags.y != 0.0f)
    {
        const float3 sampledVelocity = SampleRange3(g_InitialVelocityMin.xyz, g_InitialVelocityMax.xyz, seed + 503u);
        initialVelocity += g_MotionFlags.z != 0.0f ? sampledVelocity : ResolveEmitterBasisVector(sampledVelocity);
    }

    if (g_MotionFlags.w != 0.0f)
    {
        const float3 radialPivotOffset = g_MotionSpaceFlags.x != 0.0f
                                         ? g_MotionRadialPivot.xyz
                                         : ResolveEmitterBasisVector(g_MotionRadialPivot.xyz);
        const float3 pivotPosition = g_Center.xyz + radialPivotOffset;
        float3 radialDirection = spawnPosition - pivotPosition;
        const float3 fallbackRadialDirection = ResolveEmitterBasisVector(
            float3(
                Hash01(seed + 307u) * 2.0f - 1.0f,
                0.25f,
                Hash01(seed + 401u) * 2.0f - 1.0f
            )
        );
        const float fallbackRadialLength = length(fallbackRadialDirection);
        const float3 normalizedFallbackRadialDirection = fallbackRadialLength < 0.0001f
                                                         ? float3(0.0f, 1.0f, 0.0f)
                                                         : fallbackRadialDirection / fallbackRadialLength;
        const float radialDirectionLength = length(radialDirection);
        radialDirection = radialDirectionLength < 0.0001f
                          ? normalizedFallbackRadialDirection
                          : radialDirection / radialDirectionLength;
        const float radialSpeed = lerp(g_MotionRadialSpeedDrag.x, g_MotionRadialSpeedDrag.y, Hash01(seed + 509u));
        initialVelocity += radialDirection * radialSpeed;
    }

    const float drag = max(0.0f, lerp(g_MotionRadialSpeedDrag.z, g_MotionRadialSpeedDrag.w, Hash01(seed + 607u)));
    const float velocityScale = IntegrateLifeScale(g_VelocityScaleByLife.x, g_VelocityScaleByLife.y, lifeProgress);
    const float dragTime = drag > 0.0001f ? (1.0f - exp(-drag * lifeAge)) / drag : lifeAge;
    const float dragDecay = drag > 0.0001f ? exp(-drag * lifeAge) : 1.0f;
    const float3 acceleration = SampleRange3(g_AccelerationMin.xyz, g_AccelerationMax.xyz, seed + 701u);
    const float3 motionOffset = initialVelocity * velocityScale * dragTime + acceleration * 0.5f * lifeAge * lifeAge;
    const float currentVelocityScale = lerp(g_VelocityScaleByLife.x, g_VelocityScaleByLife.y, lifeProgress);
    const float3 currentVelocity = initialVelocity * currentVelocityScale * dragDecay + acceleration * lifeAge;
    const float currentVelocityLength = length(currentVelocity);
    const float3 velocityAlignmentHint = currentVelocityLength < 0.0001f
                                         ? float3(0.0f, 0.0f, 0.0f)
                                         : currentVelocity / currentVelocityLength;

    const float initialRotation = radians(lerp(g_RotationInitialRate.x, g_RotationInitialRate.y, Hash01(seed + 809u)));
    const float rotationOverLife = radians(EvaluateRotationOverLifeDegrees(lifeProgress));
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
            frameIndex = startFrame;
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
    instance.look = float4(velocityAlignmentHint, 0.0f);
    instance.translation = float4(spawnPosition + motionOffset, 1.0f);
    instance.lifeTime = float2(lifeMax, lifeAge);
    instance.startColor = lerp(g_StartColorMin, g_StartColorMax, colorRatio);
    instance.endColor = lerp(g_EndColorMin, g_EndColorMax, colorRatio);
    instance.subUVRect = float4(u0, v0, u0 + invCols, v0 + invRows);

    if (index >= stableDrawCount)
    {
        LifecycleState[index] = lifecycle;
        return;
    }

    OutputData[index] = instance;
    LifecycleState[index] = lifecycle;
}
