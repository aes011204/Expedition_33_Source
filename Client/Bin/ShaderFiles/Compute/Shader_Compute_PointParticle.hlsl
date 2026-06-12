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
    uint placementIndex;
    uint placementCount;
    uint reserved0;
    uint reserved1;
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
    float4 g_InitialLocationMin;                 // xyz: InitialLocation min offset, w: enabled
    float4 g_InitialLocationMax;                 // xyz: InitialLocation max offset
    float4 g_SphereLocationOffsetRadius;         // xyz: sphere center offset, w: radius
    float4 g_SphereLocationParams;               // x: enabled, y: mode(0 volume, 1 surface), z: placement mode(0 random, 1 even index)
    float4 g_PlaneRadialLocationParams;          // x: enabled, y: plane, z: shape, w: placement mode
    float4 g_PlaneRadialLocationOffsetThickness; // xyz: offset, w: thickness
    float4 g_PlaneRadialLocationRectRange;       // x/y: U range, z/w: V range
    float4 g_PlaneRadialLocationPolarRange;      // x/y: radius range, z/w: angle degree range
    uint4 g_PlaneRadialLocationSeedSalts;        // x: U/radius, y: V/angle, z/w: unused
    float4 g_PlaneRadialOrientationParams;       // x: enabled, y: target kind, z: orientation mode
    float4 g_SizeMin;                            // x/y: width/height min
    float4 g_SizeMax;                            // x/y: width/height max
    float4 g_LifeTimeParams;                     // x/y: lifetime min/max
    float4 g_LifetimeCurveParams;                // x: key count, y: enabled, z: sampling phase
    float4 g_LifetimeCurveTimes;
    float4 g_LifetimeCurveTimesBlock1;
    float4 g_LifetimeCurveValues;
    float4 g_LifetimeCurveValuesBlock1;
    float4 g_LifetimeCurveArriveTangents;
    float4 g_LifetimeCurveArriveTangentsBlock1;
    float4 g_LifetimeCurveLeaveTangents;
    float4 g_LifetimeCurveLeaveTangentsBlock1;
    float4 g_LifetimeCurveModes;
    float4 g_LifetimeCurveModesBlock1;
    float4 g_StartColorMin;
    float4 g_StartColorMax;
    float4 g_EndColorMin;
    float4 g_EndColorMax;
    float4 g_ColorOverLifeCurveParams; // x: color key count, y: alpha key count, z: color enabled, w: alpha enabled
    float4 g_ColorOverLifeColorCurveTimes;
    float4 g_ColorOverLifeColorCurveTimesBlock1;
    float4 g_ColorOverLifeColorCurveValuesR;
    float4 g_ColorOverLifeColorCurveValuesRBlock1;
    float4 g_ColorOverLifeColorCurveValuesG;
    float4 g_ColorOverLifeColorCurveValuesGBlock1;
    float4 g_ColorOverLifeColorCurveValuesB;
    float4 g_ColorOverLifeColorCurveValuesBBlock1;
    float4 g_ColorOverLifeColorCurveArriveR;
    float4 g_ColorOverLifeColorCurveArriveRBlock1;
    float4 g_ColorOverLifeColorCurveArriveG;
    float4 g_ColorOverLifeColorCurveArriveGBlock1;
    float4 g_ColorOverLifeColorCurveArriveB;
    float4 g_ColorOverLifeColorCurveArriveBBlock1;
    float4 g_ColorOverLifeColorCurveLeaveR;
    float4 g_ColorOverLifeColorCurveLeaveRBlock1;
    float4 g_ColorOverLifeColorCurveLeaveG;
    float4 g_ColorOverLifeColorCurveLeaveGBlock1;
    float4 g_ColorOverLifeColorCurveLeaveB;
    float4 g_ColorOverLifeColorCurveLeaveBBlock1;
    float4 g_ColorOverLifeColorCurveModes;
    float4 g_ColorOverLifeColorCurveModesBlock1;
    float4 g_ColorOverLifeAlphaCurveTimes;
    float4 g_ColorOverLifeAlphaCurveTimesBlock1;
    float4 g_ColorOverLifeAlphaCurveValues;
    float4 g_ColorOverLifeAlphaCurveValuesBlock1;
    float4 g_ColorOverLifeAlphaCurveArrive;
    float4 g_ColorOverLifeAlphaCurveArriveBlock1;
    float4 g_ColorOverLifeAlphaCurveLeave;
    float4 g_ColorOverLifeAlphaCurveLeaveBlock1;
    float4 g_ColorOverLifeAlphaCurveModes;
    float4 g_ColorOverLifeAlphaCurveModesBlock1;
    float4 g_SubUVParams;          // x/y: rows/cols, z: playback mode, w: frames per second
    float4 g_SubUVFrameParams;     // x/y: start/end frame, z: loop, w: enabled
    float4 g_SubUVFrameCurveTimes; // SubUV frame curve key times
    float4 g_SubUVFrameCurveTimesBlock1;
    float4 g_SubUVFrameCurveValues; // SubUV frame curve frame values
    float4 g_SubUVFrameCurveValuesBlock1;
    float4 g_SubUVFrameCurveParams; // x: key count
    uint4 g_SpawnSeedSalts;         // x: Lifetime, y: InitialLocation, z: SphereLocation, w: unused
    uint4 g_SubUVSeedSalts;         // x: SubUV RandomFrame
    uint4 g_AppearanceSeedSalts0;   // x: InitialSize, y: InitialColor RGB, z: InitialColor Alpha, w: ColorOverLife RGB
    uint4 g_AppearanceSeedSalts1;   // x: ColorOverLife Alpha
    float4 g_SizeByLifeParams;      // x/y: width start/end, z/w: height start/end
    float4 g_SizeByLifeCurveTimes;  // scaleOverLife key times
    float4 g_SizeByLifeCurveTimesBlock1;
    float4 g_SizeByLifeCurveValuesX; // scaleOverLife X multipliers
    float4 g_SizeByLifeCurveValuesXBlock1;
    float4 g_SizeByLifeCurveValuesY; // scaleOverLife Y multipliers
    float4 g_SizeByLifeCurveValuesYBlock1;
    float4 g_SizeByLifeCurveParams; // x: key count, y: multiplyX, z: multiplyY, w: axisLock
    float4 g_MotionFlags;           // x: motion enabled, y: vector enabled, z: vector world, w: radial enabled
    float4 g_MotionSpaceFlags;      // x: radial world, y: cone enabled, z: cone world, w: radial center direction mode
    float4 g_InitialVelocityMin;    // xyz: vector velocity min
    float4 g_InitialVelocityMax;    // xyz: vector velocity max
    float4 g_MotionRadialPivot;     // xyz: radial pivot
    float4 g_MotionRadialSpeedDrag; // x/y: radial speed min/max, z/w: drag min/max
    float4 g_VelocityConeAxisAngle; // xyz: cone axis, w: half-angle degrees
    float4 g_VelocityConeSpeed;     // x/y: speed min/max
    float4 g_AccelerationMin;
    float4 g_AccelerationMax;
    float4 g_AccelerationCurveTimes; // Acceleration key times
    float4 g_AccelerationCurveTimesBlock1;
    float4 g_AccelerationCurveValuesX; // Acceleration X key values
    float4 g_AccelerationCurveValuesXBlock1;
    float4 g_AccelerationCurveValuesY; // Acceleration Y key values
    float4 g_AccelerationCurveValuesYBlock1;
    float4 g_AccelerationCurveValuesZ; // Acceleration Z key values
    float4 g_AccelerationCurveValuesZBlock1;
    float4 g_AccelerationCurveParams; // x: key count, y: enabled
    float4 g_VelocityScaleByLife;
    float4 g_VelocityScaleByLifeCurveTimes; // VelocityOverLife key times
    float4 g_VelocityScaleByLifeCurveTimesBlock1;
    float4 g_VelocityScaleByLifeCurveValues; // VelocityOverLife multiplier values
    float4 g_VelocityScaleByLifeCurveValuesBlock1;
    float4 g_VelocityScaleByLifeCurveParams; // x: key count
    float4 g_OrbitParams;                    // x: enabled, y: plane
    float4 g_OrbitAngleRange;                // x/y: angle start/end degrees
    float4 g_OrbitAngleCurveTimes;           // OrbitOverLife angle key times
    float4 g_OrbitAngleCurveTimesBlock1;
    float4 g_OrbitAngleCurveValues; // OrbitOverLife angle degree values
    float4 g_OrbitAngleCurveValuesBlock1;
    float4 g_OrbitAngleCurveParams; // x: key count
    float4 g_OrbitRadiusRange;      // x/y: radius scale start/end
    float4 g_OrbitRadiusCurveTimes; // OrbitOverLife radius scale key times
    float4 g_OrbitRadiusCurveTimesBlock1;
    float4 g_OrbitRadiusCurveValues; // OrbitOverLife radius scale values
    float4 g_OrbitRadiusCurveValuesBlock1;
    float4 g_OrbitRadiusCurveParams;     // x: key count
    uint4 g_MotionSeedSalts0;            // x: InitialVelocity, y: InitialRadialVelocity, z: VelocityCone, w: Acceleration
    uint4 g_MotionSeedSalts1;            // x: Drag
    float4 g_RotationInitialRate;        // x/y: initial rotation min/max, z/w: rate min/max
    float4 g_RotationByLife;             // (Legacy) x/y: rotation start/end, z/w: rate scale start/end
    float4 g_RotationOverLifeCurveTimes; // rotationOverLife key times
    float4 g_RotationOverLifeCurveTimesBlock1;
    float4 g_RotationOverLifeCurveValues; // rotationOverLife degree values
    float4 g_RotationOverLifeCurveValuesBlock1;
    float4 g_RotationOverLifeCurveParams;       // x: key count
    float4 g_RotationRateScaleByLifeCurveTimes; // rotationRateScaleByLife key times
    float4 g_RotationRateScaleByLifeCurveTimesBlock1;
    float4 g_RotationRateScaleByLifeCurveValues; // rotationRateScaleByLife multiplier values
    float4 g_RotationRateScaleByLifeCurveValuesBlock1;
    float4 g_RotationRateScaleByLifeCurveParams; // x: key count
    uint4 g_RotationSeedSalts;                   // x: InitialRotation, y: InitialRotationRate
};

RWStructuredBuffer<ParticleInstanceVertex> OutputData : register(u0);
RWStructuredBuffer<ParticleLifecycleState> LifecycleState : register(u2);

static const float kPlaneRadialAngleEpsilon = 1e-4f;
static const uint kEffectDistributionCurveMaxKeys = 8u;

float ReadVec4Component(float4 values, uint index)
{
    float result = values.x;
    if (index == 1u)
        result = values.y;
    else if (index == 2u)
        result = values.z;
    else if (index == 3u)
        result = values.w;

    return result;
}

float ReadVec4Component(float4 valuesBlock0, float4 valuesBlock1, uint index)
{
    return index < 4u
           ? ReadVec4Component(valuesBlock0, index)
           : ReadVec4Component(valuesBlock1, index - 4u);
}

void AccumulateAccelerationSegment(
    float leftTime,
    float rightTime,
    float leftValue,
    float rightValue,
    float lifeProgress,
    inout float integral,
    inout float weightedIntegral)
{
    const float segmentEnd = min(lifeProgress, rightTime);
    if (segmentEnd <= leftTime)
        return;

    const float width = max(0.0001f, rightTime - leftTime);
    const float slope = (rightValue - leftValue) / width;
    const float dt = segmentEnd - leftTime;
    const float start2 = leftTime * leftTime;
    const float end2 = segmentEnd * segmentEnd;
    const float start3 = start2 * leftTime;
    const float end3 = end2 * segmentEnd;

    integral += leftValue * dt + 0.5f * slope * dt * dt;
    weightedIntegral +=
        0.5f * leftValue * (end2 - start2) +
        slope * ((end3 - start3) / 3.0f - 0.5f * leftTime * (end2 - start2));
}

void IntegrateAccelerationAxis(
    float lifeProgress,
    uint keyCount,
    float4 times,
    float4 timesBlock1,
    float4 values,
    float4 valuesBlock1,
    out float velocityProgress,
    out float displacementProgress)
{
    keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, keyCount));
    lifeProgress = saturate(lifeProgress);

    float integral = 0.0f;
    float weightedIntegral = 0.0f;
    const float firstTime = saturate(ReadVec4Component(times, timesBlock1, 0u));
    const float firstValue = ReadVec4Component(values, valuesBlock1, 0u);

    AccumulateAccelerationSegment(0.0f, firstTime, firstValue, firstValue, lifeProgress, integral, weightedIntegral);

    [loop]
    for (uint keyIndex = 1u; keyIndex < keyCount; ++keyIndex)
    {
        const float leftTime = saturate(ReadVec4Component(times, timesBlock1, keyIndex - 1u));
        const float rightTime = saturate(ReadVec4Component(times, timesBlock1, keyIndex));
        const float leftValue = ReadVec4Component(values, valuesBlock1, keyIndex - 1u);
        const float rightValue = ReadVec4Component(values, valuesBlock1, keyIndex);
        AccumulateAccelerationSegment(leftTime, rightTime, leftValue, rightValue, lifeProgress, integral, weightedIntegral);
    }

    const float lastTime = saturate(ReadVec4Component(times, timesBlock1, keyCount - 1u));
    const float lastValue = ReadVec4Component(values, valuesBlock1, keyCount - 1u);
    AccumulateAccelerationSegment(lastTime, lifeProgress, lastValue, lastValue, lifeProgress, integral, weightedIntegral);

    velocityProgress = integral;
    displacementProgress = lifeProgress * integral - weightedIntegral;
}

void IntegrateAccelerationCurve(
    float lifeProgress,
    out float3 velocityProgress,
    out float3 displacementProgress)
{
    const uint keyCount = (uint)g_AccelerationCurveParams.x;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float velocityZ = 0.0f;
    float displacementX = 0.0f;
    float displacementY = 0.0f;
    float displacementZ = 0.0f;
    IntegrateAccelerationAxis(
        lifeProgress,
        keyCount,
        g_AccelerationCurveTimes,
        g_AccelerationCurveTimesBlock1,
        g_AccelerationCurveValuesX,
        g_AccelerationCurveValuesXBlock1,
        velocityX,
        displacementX
    );
    IntegrateAccelerationAxis(
        lifeProgress,
        keyCount,
        g_AccelerationCurveTimes,
        g_AccelerationCurveTimesBlock1,
        g_AccelerationCurveValuesY,
        g_AccelerationCurveValuesYBlock1,
        velocityY,
        displacementY
    );
    IntegrateAccelerationAxis(
        lifeProgress,
        keyCount,
        g_AccelerationCurveTimes,
        g_AccelerationCurveTimesBlock1,
        g_AccelerationCurveValuesZ,
        g_AccelerationCurveValuesZBlock1,
        velocityZ,
        displacementZ
    );
    velocityProgress = float3(velocityX, velocityY, velocityZ);
    displacementProgress = float3(displacementX, displacementY, displacementZ);
}

float EvaluateCompactCurve(
    float lifeProgress,
    uint keyCount,
    float4 times,
    float4 timesBlock1,
    float4 values,
    float4 valuesBlock1,
    float4 arriveTangents,
    float4 arriveTangentsBlock1,
    float4 leaveTangents,
    float4 leaveTangentsBlock1,
    float4 modes,
    float4 modesBlock1,
    float fallbackValue)
{
    keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, keyCount));

    bool found = false;
    float result = ReadVec4Component(values, valuesBlock1, 0u);
    if (keyCount > 1u && lifeProgress > ReadVec4Component(times, timesBlock1, 0u))
    {
        result = fallbackValue;
        [loop]
        for (uint keyIndex = 1u; keyIndex < kEffectDistributionCurveMaxKeys; ++keyIndex)
        {
            if (keyIndex >= keyCount || found)
                break;

            const float rightTime = ReadVec4Component(times, timesBlock1, keyIndex);
            if (lifeProgress > rightTime)
                continue;

            const float leftTime = ReadVec4Component(times, timesBlock1, keyIndex - 1u);
            const float leftValue = ReadVec4Component(values, valuesBlock1, keyIndex - 1u);
            const float rightValue = ReadVec4Component(values, valuesBlock1, keyIndex);
            const float mode = ReadVec4Component(modes, modesBlock1, keyIndex - 1u);
            const float width = max(0.0001f, rightTime - leftTime);
            const float ratio = saturate((lifeProgress - leftTime) / width);

            if (mode < 0.5f)
                result = leftValue;
            else if (mode >= 1.5f)
            {
                const float t2 = ratio * ratio;
                const float t3 = t2 * ratio;
                const float leftLeave = ReadVec4Component(leaveTangents, leaveTangentsBlock1, keyIndex - 1u) * width;
                const float rightArrive = ReadVec4Component(arriveTangents, arriveTangentsBlock1, keyIndex) * width;
                result =
                    (2.0f * t3 - 3.0f * t2 + 1.0f) * leftValue +
                    (t3 - 2.0f * t2 + ratio) * leftLeave +
                    (-2.0f * t3 + 3.0f * t2) * rightValue +
                    (t3 - t2) * rightArrive;
            }
            else
                result = lerp(leftValue, rightValue, ratio);

            found = true;
            break;
        }

        const float lastTime = ReadVec4Component(times, timesBlock1, keyCount - 1u);
        if (!found && lifeProgress > lastTime)
            result = ReadVec4Component(values, valuesBlock1, keyCount - 1u);
    }

    return result;
}

float4 EvaluateColorOverLife(float lifeProgress, float4 fallbackColor)
{
    float4 result = fallbackColor;

    if (g_ColorOverLifeCurveParams.z != 0.0f)
    {
        const uint colorKeyCount = (uint)g_ColorOverLifeCurveParams.x;
        result.rgb = saturate(
            float3(
                EvaluateCompactCurve(
                    lifeProgress,
                    colorKeyCount,
                    g_ColorOverLifeColorCurveTimes,
                    g_ColorOverLifeColorCurveTimesBlock1,
                    g_ColorOverLifeColorCurveValuesR,
                    g_ColorOverLifeColorCurveValuesRBlock1,
                    g_ColorOverLifeColorCurveArriveR,
                    g_ColorOverLifeColorCurveArriveRBlock1,
                    g_ColorOverLifeColorCurveLeaveR,
                    g_ColorOverLifeColorCurveLeaveRBlock1,
                    g_ColorOverLifeColorCurveModes,
                    g_ColorOverLifeColorCurveModesBlock1,
                    fallbackColor.r
                ),
                EvaluateCompactCurve(
                    lifeProgress,
                    colorKeyCount,
                    g_ColorOverLifeColorCurveTimes,
                    g_ColorOverLifeColorCurveTimesBlock1,
                    g_ColorOverLifeColorCurveValuesG,
                    g_ColorOverLifeColorCurveValuesGBlock1,
                    g_ColorOverLifeColorCurveArriveG,
                    g_ColorOverLifeColorCurveArriveGBlock1,
                    g_ColorOverLifeColorCurveLeaveG,
                    g_ColorOverLifeColorCurveLeaveGBlock1,
                    g_ColorOverLifeColorCurveModes,
                    g_ColorOverLifeColorCurveModesBlock1,
                    fallbackColor.g
                ),
                EvaluateCompactCurve(
                    lifeProgress,
                    colorKeyCount,
                    g_ColorOverLifeColorCurveTimes,
                    g_ColorOverLifeColorCurveTimesBlock1,
                    g_ColorOverLifeColorCurveValuesB,
                    g_ColorOverLifeColorCurveValuesBBlock1,
                    g_ColorOverLifeColorCurveArriveB,
                    g_ColorOverLifeColorCurveArriveBBlock1,
                    g_ColorOverLifeColorCurveLeaveB,
                    g_ColorOverLifeColorCurveLeaveBBlock1,
                    g_ColorOverLifeColorCurveModes,
                    g_ColorOverLifeColorCurveModesBlock1,
                    fallbackColor.b
                )
            )
        );
    }

    if (g_ColorOverLifeCurveParams.w != 0.0f)
    {
        const uint alphaKeyCount = (uint)g_ColorOverLifeCurveParams.y;
        result.a = saturate(
            EvaluateCompactCurve(
                lifeProgress,
                alphaKeyCount,
                g_ColorOverLifeAlphaCurveTimes,
                g_ColorOverLifeAlphaCurveTimesBlock1,
                g_ColorOverLifeAlphaCurveValues,
                g_ColorOverLifeAlphaCurveValuesBlock1,
                g_ColorOverLifeAlphaCurveArrive,
                g_ColorOverLifeAlphaCurveArriveBlock1,
                g_ColorOverLifeAlphaCurveLeave,
                g_ColorOverLifeAlphaCurveLeaveBlock1,
                g_ColorOverLifeAlphaCurveModes,
                g_ColorOverLifeAlphaCurveModesBlock1,
                fallbackColor.a
            )
        );
    }

    return result;
}

float2 EvaluateSizeByLifeMultiplier(float lifeProgress)
{
    const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)g_SizeByLifeCurveParams.x));
    float2 multiplier = float2(g_SizeByLifeParams.x, g_SizeByLifeParams.z);

    if (keyCount == 1u || lifeProgress <= ReadVec4Component(g_SizeByLifeCurveTimes, g_SizeByLifeCurveTimesBlock1, 0u))
    {
        multiplier = float2(
            ReadVec4Component(g_SizeByLifeCurveValuesX, g_SizeByLifeCurveValuesXBlock1, 0u),
            ReadVec4Component(g_SizeByLifeCurveValuesY, g_SizeByLifeCurveValuesYBlock1, 0u)
        );
    }
    else
    {
        bool found = false;
        [loop]
        for (uint keyIndex = 1u; keyIndex < kEffectDistributionCurveMaxKeys; ++keyIndex)
        {
            if (keyIndex >= keyCount || found)
                break;

            const float rightTime = ReadVec4Component(g_SizeByLifeCurveTimes, g_SizeByLifeCurveTimesBlock1, keyIndex);
            if (lifeProgress > rightTime)
                continue;

            const float leftTime = ReadVec4Component(g_SizeByLifeCurveTimes, g_SizeByLifeCurveTimesBlock1, keyIndex - 1u);
            const float ratio = saturate((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime));
            multiplier = float2(
                lerp(
                    ReadVec4Component(g_SizeByLifeCurveValuesX, g_SizeByLifeCurveValuesXBlock1, keyIndex - 1u),
                    ReadVec4Component(g_SizeByLifeCurveValuesX, g_SizeByLifeCurveValuesXBlock1, keyIndex),
                    ratio
                ),
                lerp(
                    ReadVec4Component(g_SizeByLifeCurveValuesY, g_SizeByLifeCurveValuesYBlock1, keyIndex - 1u),
                    ReadVec4Component(g_SizeByLifeCurveValuesY, g_SizeByLifeCurveValuesYBlock1, keyIndex),
                    ratio
                )
            );
            found = true;
            break;
        }

        const float lastTime = ReadVec4Component(g_SizeByLifeCurveTimes, g_SizeByLifeCurveTimesBlock1, keyCount - 1u);
        if (!found && lifeProgress > lastTime)
        {
            multiplier = float2(
                ReadVec4Component(g_SizeByLifeCurveValuesX, g_SizeByLifeCurveValuesXBlock1, keyCount - 1u),
                ReadVec4Component(g_SizeByLifeCurveValuesY, g_SizeByLifeCurveValuesYBlock1, keyCount - 1u)
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
    const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)g_RotationOverLifeCurveParams.x));
    float rotationDegrees = g_RotationByLife.x;

    if (keyCount == 1u || lifeProgress <= ReadVec4Component(g_RotationOverLifeCurveTimes, g_RotationOverLifeCurveTimesBlock1, 0u))
        rotationDegrees = ReadVec4Component(g_RotationOverLifeCurveValues, g_RotationOverLifeCurveValuesBlock1, 0u);
    else
    {
        bool found = false;
        [loop]
        for (uint keyIndex = 1u; keyIndex < kEffectDistributionCurveMaxKeys; ++keyIndex)
        {
            if (keyIndex >= keyCount || found)
                break;

            const float rightTime = ReadVec4Component(g_RotationOverLifeCurveTimes, g_RotationOverLifeCurveTimesBlock1, keyIndex);
            if (lifeProgress > rightTime)
                continue;

            const float leftTime = ReadVec4Component(g_RotationOverLifeCurveTimes, g_RotationOverLifeCurveTimesBlock1, keyIndex - 1u);
            const float ratio = saturate((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime));
            rotationDegrees = lerp(
                ReadVec4Component(g_RotationOverLifeCurveValues, g_RotationOverLifeCurveValuesBlock1, keyIndex - 1u),
                ReadVec4Component(g_RotationOverLifeCurveValues, g_RotationOverLifeCurveValuesBlock1, keyIndex),
                ratio
            );
            found = true;
            break;
        }

        const float lastTime = ReadVec4Component(g_RotationOverLifeCurveTimes, g_RotationOverLifeCurveTimesBlock1, keyCount - 1u);
        if (!found && lifeProgress > lastTime)
            rotationDegrees = ReadVec4Component(g_RotationOverLifeCurveValues, g_RotationOverLifeCurveValuesBlock1, keyCount - 1u);
    }

    return rotationDegrees;
}

float IntegrateLinearCurveSegment(float leftTime, float leftValue, float rightTime, float rightValue, float startTime, float endTime)
{
    const float width = max(0.0001f, rightTime - leftTime);
    const float t0 = saturate((startTime - leftTime) / width);
    const float t1 = saturate((endTime - leftTime) / width);
    const float valueDelta = rightValue - leftValue;
    return width * (leftValue * (t1 - t0) + 0.5f * valueDelta * (t1 * t1 - t0 * t0));
}

// RotationRateScaleByLife curve payload를 0..lifeProgress 구간의 평균 배율로 적분 평가한다.
float EvaluateRotationRateScaleByLifeAverage(float lifeProgress)
{
    const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)g_RotationRateScaleByLifeCurveParams.x));
    const float clampedProgress = saturate(lifeProgress);
    const float firstTime = saturate(ReadVec4Component(g_RotationRateScaleByLifeCurveTimes, g_RotationRateScaleByLifeCurveTimesBlock1, 0u));
    const float firstValue = ReadVec4Component(g_RotationRateScaleByLifeCurveValues, g_RotationRateScaleByLifeCurveValuesBlock1, 0u);

    float result = firstValue;
    if (keyCount > 1u && clampedProgress > firstTime && clampedProgress > 0.0001f)
    {
        float area = firstValue * firstTime;
        float previousTime = firstTime;
        float previousValue = firstValue;
        bool done = false;

        [loop]
        for (uint keyIndex = 1u; keyIndex < kEffectDistributionCurveMaxKeys; ++keyIndex)
        {
            if (keyIndex >= keyCount || done)
                break;

            const float rightTime = max(
                previousTime,
                saturate(ReadVec4Component(g_RotationRateScaleByLifeCurveTimes, g_RotationRateScaleByLifeCurveTimesBlock1, keyIndex))
            );
            const float rightValue = ReadVec4Component(g_RotationRateScaleByLifeCurveValues, g_RotationRateScaleByLifeCurveValuesBlock1, keyIndex);
            const float segmentEnd = min(clampedProgress, rightTime);
            if (segmentEnd > previousTime)
            {
                area += IntegrateLinearCurveSegment(
                    previousTime,
                    previousValue,
                    rightTime,
                    rightValue,
                    previousTime,
                    segmentEnd
                );
            }

            if (clampedProgress <= rightTime)
            {
                done = true;
                break;
            }

            previousTime = rightTime;
            previousValue = rightValue;
        }

        if (!done && clampedProgress > previousTime)
            area += previousValue * (clampedProgress - previousTime);

        result = area / max(0.0001f, clampedProgress);
    }

    return result;
}

float EvaluateVelocityScaleByLife(float lifeProgress)
{
    const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)g_VelocityScaleByLifeCurveParams.x));
    const float clampedProgress = saturate(lifeProgress);
    float velocityScale = g_VelocityScaleByLife.x;

    if (keyCount == 1u || clampedProgress <= ReadVec4Component(g_VelocityScaleByLifeCurveTimes, g_VelocityScaleByLifeCurveTimesBlock1, 0u))
        velocityScale = ReadVec4Component(g_VelocityScaleByLifeCurveValues, g_VelocityScaleByLifeCurveValuesBlock1, 0u);
    else
    {
        bool found = false;
        [loop]
        for (uint keyIndex = 1u; keyIndex < kEffectDistributionCurveMaxKeys; ++keyIndex)
        {
            if (keyIndex >= keyCount || found)
                break;

            const float rightTime = ReadVec4Component(g_VelocityScaleByLifeCurveTimes, g_VelocityScaleByLifeCurveTimesBlock1, keyIndex);
            if (clampedProgress > rightTime)
                continue;

            const float leftTime = ReadVec4Component(g_VelocityScaleByLifeCurveTimes, g_VelocityScaleByLifeCurveTimesBlock1, keyIndex - 1u);
            const float ratio = saturate((clampedProgress - leftTime) / max(0.0001f, rightTime - leftTime));
            velocityScale = lerp(
                ReadVec4Component(g_VelocityScaleByLifeCurveValues, g_VelocityScaleByLifeCurveValuesBlock1, keyIndex - 1u),
                ReadVec4Component(g_VelocityScaleByLifeCurveValues, g_VelocityScaleByLifeCurveValuesBlock1, keyIndex),
                ratio
            );
            found = true;
            break;
        }

        const float lastTime = ReadVec4Component(g_VelocityScaleByLifeCurveTimes, g_VelocityScaleByLifeCurveTimesBlock1, keyCount - 1u);
        if (!found && clampedProgress > lastTime)
            velocityScale = ReadVec4Component(g_VelocityScaleByLifeCurveValues, g_VelocityScaleByLifeCurveValuesBlock1, keyCount - 1u);
    }

    return velocityScale;
}

float EvaluateVelocityScaleByLifeAverage(float lifeProgress)
{
    const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)g_VelocityScaleByLifeCurveParams.x));
    const float clampedProgress = saturate(lifeProgress);
    const float firstTime = saturate(ReadVec4Component(g_VelocityScaleByLifeCurveTimes, g_VelocityScaleByLifeCurveTimesBlock1, 0u));
    const float firstValue = ReadVec4Component(g_VelocityScaleByLifeCurveValues, g_VelocityScaleByLifeCurveValuesBlock1, 0u);

    float result = firstValue;
    if (keyCount > 1u && clampedProgress > firstTime && clampedProgress > 0.0001f)
    {
        float area = firstValue * firstTime;
        float previousTime = firstTime;
        float previousValue = firstValue;
        bool done = false;

        [loop]
        for (uint keyIndex = 1u; keyIndex < kEffectDistributionCurveMaxKeys; ++keyIndex)
        {
            if (keyIndex >= keyCount || done)
                break;

            const float rightTime = max(
                previousTime,
                saturate(ReadVec4Component(g_VelocityScaleByLifeCurveTimes, g_VelocityScaleByLifeCurveTimesBlock1, keyIndex))
            );
            const float rightValue = ReadVec4Component(g_VelocityScaleByLifeCurveValues, g_VelocityScaleByLifeCurveValuesBlock1, keyIndex);
            const float segmentEnd = min(clampedProgress, rightTime);
            if (segmentEnd > previousTime)
            {
                area += IntegrateLinearCurveSegment(
                    previousTime,
                    previousValue,
                    rightTime,
                    rightValue,
                    previousTime,
                    segmentEnd
                );
            }

            if (clampedProgress <= rightTime)
            {
                done = true;
                break;
            }

            previousTime = rightTime;
            previousValue = rightValue;
        }

        if (!done && clampedProgress > previousTime)
            area += previousValue * (clampedProgress - previousTime);

        result = area / max(0.0001f, clampedProgress);
    }

    return result;
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

float Resolve_EvenByParticleIndexAngleRatio(uint particleIndex, uint maxActiveCount, float angleMinDegrees, float angleMaxDegrees)
{
    const uint clampedCount = max(1u, maxActiveCount);
    const float angleSpanDegrees = abs(angleMaxDegrees - angleMinDegrees);
    const bool isFullCircle = abs(angleSpanDegrees - 360.0f) <= kPlaneRadialAngleEpsilon;
    const float denominator = isFullCircle ? (float)clampedCount : (float)max(1u, clampedCount - 1u);
    const float ratio = (float)particleIndex / denominator;

    return clampedCount <= 1u ? 0.0f : ratio;
}

float3 ResolveEvenByParticleIndexSphereDirection(uint placementIndex, uint placementCount)
{
    const uint clampedCount = max(1u, placementCount);
    const uint clampedIndex = placementIndex % clampedCount;
    const float ratio = ((float)clampedIndex + 0.5f) / (float)clampedCount;
    const float z = 1.0f - 2.0f * ratio;
    const float radiusOnPlane = sqrt(max(0.0f, 1.0f - z * z));
    const float goldenAngle = 3.14159265359f * (3.0f - sqrt(5.0f));
    const float angle = (float)clampedIndex * goldenAngle;

    const float3 direction = float3(cos(angle) * radiusOnPlane, sin(angle) * radiusOnPlane, z);
    return placementCount <= 1u ? float3(0.0f, 0.0f, 1.0f) : direction;
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

float3 ResolveEmitterLocalVector(float3 worldValue)
{
    const float rightLength = length(g_EmitterBasisRight.xyz);
    const float upLength = length(g_EmitterBasisUp.xyz);
    const float lookLength = length(g_EmitterBasisLook.xyz);
    const float3 right = rightLength < 0.0001f ? float3(1.0f, 0.0f, 0.0f) : g_EmitterBasisRight.xyz / rightLength;
    const float3 up = upLength < 0.0001f ? float3(0.0f, 1.0f, 0.0f) : g_EmitterBasisUp.xyz / upLength;
    const float3 look = lookLength < 0.0001f ? float3(0.0f, 0.0f, 1.0f) : g_EmitterBasisLook.xyz / lookLength;
    return float3(dot(worldValue, right), dot(worldValue, up), dot(worldValue, look));
}

float EvaluateOrbitCurve(float lifeProgress, uint keyCount, float4 times, float4 timesBlock1, float4 values, float4 valuesBlock1, float fallbackValue)
{
    keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, keyCount));

    bool found = false;
    float result = fallbackValue;
    if (keyCount == 1u || lifeProgress <= ReadVec4Component(times, timesBlock1, 0u))
    {
        result = ReadVec4Component(values, valuesBlock1, 0u);
        found = true;
    }
    else
    {
        [loop]
        for (uint keyIndex = 1u; keyIndex < kEffectDistributionCurveMaxKeys; ++keyIndex)
        {
            if (keyIndex >= keyCount || found)
                break;

            const float rightTime = ReadVec4Component(times, timesBlock1, keyIndex);
            if (lifeProgress > rightTime)
                continue;

            const float leftTime = ReadVec4Component(times, timesBlock1, keyIndex - 1u);
            const float ratio = saturate((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime));
            result = lerp(
                ReadVec4Component(values, valuesBlock1, keyIndex - 1u),
                ReadVec4Component(values, valuesBlock1, keyIndex),
                ratio
            );
            found = true;
            break;
        }
    }

    const float lastTime = ReadVec4Component(times, timesBlock1, keyCount - 1u);
    if (!found && lifeProgress > lastTime)
        result = ReadVec4Component(values, valuesBlock1, keyCount - 1u);

    return result;
}

float3 ApplyOrbitOverLife(float3 spawnOffset, float lifeProgress)
{
    float3 result = spawnOffset;
    if (g_OrbitParams.x != 0.0f)
    {
        float3 localOffset = ResolveEmitterLocalVector(spawnOffset);
        const float angleDegrees = EvaluateOrbitCurve(
            lifeProgress,
            (uint)g_OrbitAngleCurveParams.x,
            g_OrbitAngleCurveTimes,
            g_OrbitAngleCurveTimesBlock1,
            g_OrbitAngleCurveValues,
            g_OrbitAngleCurveValuesBlock1,
            lerp(g_OrbitAngleRange.x, g_OrbitAngleRange.y, lifeProgress)
        );
        const float radiusScale = EvaluateOrbitCurve(
            lifeProgress,
            (uint)g_OrbitRadiusCurveParams.x,
            g_OrbitRadiusCurveTimes,
            g_OrbitRadiusCurveTimesBlock1,
            g_OrbitRadiusCurveValues,
            g_OrbitRadiusCurveValuesBlock1,
            lerp(g_OrbitRadiusRange.x, g_OrbitRadiusRange.y, lifeProgress)
        );

        const float c = cos(radians(angleDegrees));
        const float s = sin(radians(angleDegrees));
        const uint plane = (uint)g_OrbitParams.y;
        float2 planeOffset = localOffset.xy;
        if (plane == 1u)
            planeOffset = localOffset.xz;
        else if (plane == 2u)
            planeOffset = localOffset.yz;

        planeOffset *= radiusScale;
        const float2 rotatedOffset = float2(
            planeOffset.x * c - planeOffset.y * s,
            planeOffset.x * s + planeOffset.y * c
        );

        if (plane == 1u)
        {
            localOffset.x = rotatedOffset.x;
            localOffset.z = rotatedOffset.y;
        }
        else if (plane == 2u)
        {
            localOffset.y = rotatedOffset.x;
            localOffset.z = rotatedOffset.y;
        }
        else
        {
            localOffset.x = rotatedOffset.x;
            localOffset.y = rotatedOffset.y;
        }

        result = ResolveEmitterBasisVector(localOffset);
    }

    return result;
}

float3 NormalizeOrUp(float3 value)
{
    const float valueLength = length(value);
    return valueLength < 0.0001f ? float3(0.0f, 1.0f, 0.0f) : value / valueLength;
}

float3 SampleConeDirection(float3 axis, float angleDegrees, uint seed)
{
    axis = NormalizeOrUp(axis);
    const float clampedAngleDegrees = clamp(angleDegrees, 0.0f, 180.0f);
    float3 sampledDirection = axis;

    if (clampedAngleDegrees > 0.0001f)
    {
        const float cosMax = cos(radians(clampedAngleDegrees));
        const float cosTheta = lerp(1.0f, cosMax, Hash01(seed + 1703u));
        const float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
        const float phi = Hash01(seed + 1721u) * 6.28318530718f;
        const float3 helper = abs(axis.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        const float3 tangent = normalize(cross(helper, axis));
        const float3 bitangent = cross(axis, tangent);
        sampledDirection = axis * cosTheta + (tangent * cos(phi) + bitangent * sin(phi)) * sinTheta;
    }

    return sampledDirection;
}

struct PlaneRadialFrame
{
    float3 localOffset;
    float3 radialDirection;
    float3 tangentCW;
    float3 tangentCCW;
    float3 planeNormal;
    float sampleAngle;
    float enabled;
};

void ResolvePlaneRadialBasis(out float3 axisU, out float3 axisV, out float3 planeNormal)
{
    const uint plane = (uint)g_PlaneRadialLocationParams.y;
    axisU = float3(1.0f, 0.0f, 0.0f);
    axisV = float3(0.0f, 1.0f, 0.0f);
    planeNormal = float3(0.0f, 0.0f, 1.0f);

    if (plane == 1u)
    {
        axisV = float3(0.0f, 0.0f, 1.0f);
        planeNormal = float3(0.0f, 1.0f, 0.0f);
    }
    else if (plane == 2u)
    {
        axisU = float3(0.0f, 1.0f, 0.0f);
        axisV = float3(0.0f, 0.0f, 1.0f);
        planeNormal = float3(1.0f, 0.0f, 0.0f);
    }
}

void SamplePlaneRadialFrame(uint seed, uint particleIndex, uint placementIndex, uint placementCount, out PlaneRadialFrame frame)
{
    float3 axisU = float3(1.0f, 0.0f, 0.0f);
    float3 axisV = float3(0.0f, 1.0f, 0.0f);
    float3 planeNormal = float3(0.0f, 0.0f, 1.0f);
    ResolvePlaneRadialBasis(axisU, axisV, planeNormal);

    frame.localOffset = float3(0.0f, 0.0f, 0.0f);
    frame.planeNormal = planeNormal;
    frame.radialDirection = axisU;
    frame.tangentCW = -axisV;
    frame.tangentCCW = axisV;
    frame.sampleAngle = 0.0f;
    frame.enabled = 0.0f;

    if (g_PlaneRadialLocationParams.x == 0.0f)
        return;

    const uint shape = (uint)g_PlaneRadialLocationParams.z;
    const uint placementMode = (uint)g_PlaneRadialLocationParams.w;
    float u = 0.0f;
    float v = 0.0f;

    if (shape == 0u)
    {
        u = lerp(g_PlaneRadialLocationRectRange.x, g_PlaneRadialLocationRectRange.y, Hash01(seed + g_PlaneRadialLocationSeedSalts.x + 1601u));
        v = lerp(g_PlaneRadialLocationRectRange.z, g_PlaneRadialLocationRectRange.w, Hash01(seed + g_PlaneRadialLocationSeedSalts.y + 1613u));
        frame.sampleAngle = atan2(v, u);
    }
    else
    {
        const float radiusMin = min(g_PlaneRadialLocationPolarRange.x, g_PlaneRadialLocationPolarRange.y);
        const float radiusMax = max(g_PlaneRadialLocationPolarRange.x, g_PlaneRadialLocationPolarRange.y);
        const float angleMinDegrees = g_PlaneRadialLocationPolarRange.z;
        const float angleMaxDegrees = g_PlaneRadialLocationPolarRange.w;
        const float angleMin = radians(angleMinDegrees);
        const float angleMax = radians(angleMaxDegrees);
        const float radius = lerp(radiusMin, radiusMax, Hash01(seed + g_PlaneRadialLocationSeedSalts.x + 1601u));
        const float angleRatio = placementMode == 1u
                                 ? Resolve_EvenByParticleIndexAngleRatio(
                                     placementIndex,
                                     placementCount,
                                     angleMinDegrees,
                                     angleMaxDegrees
                                 )
                                 : Hash01(seed + g_PlaneRadialLocationSeedSalts.y + 1613u);
        const float angle = lerp(angleMin, angleMax, angleRatio);
        frame.sampleAngle = angle;
        u = cos(angle) * radius;
        v = sin(angle) * radius;
    }

    const float thickness = max(0.0f, g_PlaneRadialLocationOffsetThickness.w);
    const float normalOffset = thickness > 0.0001f
                               ? (Hash01(seed + 1621u) - 0.5f) * thickness
                               : 0.0f;
    const float3 radialVector = axisU * u + axisV * v;
    const float radialLength = length(radialVector);
    frame.radialDirection = radialLength < 0.0001f
                            ? normalize(axisU * cos(frame.sampleAngle) + axisV * sin(frame.sampleAngle))
                            : radialVector / radialLength;
    frame.tangentCW = normalize(axisU * sin(frame.sampleAngle) - axisV * cos(frame.sampleAngle));
    frame.tangentCCW = -frame.tangentCW;
    frame.localOffset = g_PlaneRadialLocationOffsetThickness.xyz + radialVector + planeNormal * normalOffset;
    frame.enabled = 1.0f;
}

float ResolvePlaneRadialSpriteRotation(float frameEnabled, float sampleAngle)
{
    float rotation = 0.0f;
    const uint orientationMode = (uint)g_PlaneRadialOrientationParams.z;
    if (frameEnabled != 0.0f && g_PlaneRadialOrientationParams.x != 0.0f && orientationMode != 0u && orientationMode != 5u)
    {
        if (orientationMode == 2u)
            rotation = sampleAngle + 3.14159265359f;
        else if (orientationMode == 3u)
            rotation = sampleAngle - 1.57079632679f;
        else if (orientationMode == 4u)
            rotation = sampleAngle + 1.57079632679f;
        else
            rotation = sampleAngle;
    }

    return rotation;
}

// sphere location 설정에 따라 구 내부/표면 spawn 오프셋을 샘플링한다.
float3 SampleSphereLocationOffset(uint seed, uint placementIndex, uint placementCount)
{
    const float radius = max(0.0f, g_SphereLocationOffsetRadius.w);
    const float twoPi = 6.28318530718f;
    const uint sphereMode = (uint)g_SphereLocationParams.y;
    const uint placementMode = (uint)g_SphereLocationParams.z;
    float3 direction;
    if (sphereMode == 1u && placementMode == 1u)
        direction = ResolveEvenByParticleIndexSphereDirection(placementIndex, placementCount);
    else
    {
        const float z = Hash01(seed + 1301u) * 2.0f - 1.0f;
        const float angle = Hash01(seed + 1409u) * twoPi;
        const float xy = sqrt(max(0.0f, 1.0f - z * z));
        direction = float3(cos(angle) * xy, sin(angle) * xy, z);
    }
    const float distanceScale = sphereMode == 1u
                                ? 1.0f
                                : pow(Hash01(seed + 1511u), 1.0f / 3.0f);
    const float enabled = g_SphereLocationParams.x != 0.0f && radius > 0.0001f ? 1.0f : 0.0f;

    return ResolveEmitterBasisVector(g_SphereLocationOffsetRadius.xyz + direction * radius * distanceScale) * enabled;
}

// 수명 min/max 범위에서 이번 particle의 최대 수명을 샘플링한다.
float SampleLifeMax(uint seed)
{
    const float lifeMin = max(0.0001f, min(g_LifeTimeParams.x, g_LifeTimeParams.y));
    const float lifeMax = max(lifeMin, max(g_LifeTimeParams.x, g_LifeTimeParams.y));
    float result = lerp(lifeMin, lifeMax, Hash01(seed + 43u));

    if (g_LifetimeCurveParams.y != 0.0f)
    {
        result = EvaluateCompactCurve(
            saturate(g_LifetimeCurveParams.z),
            (uint)g_LifetimeCurveParams.x,
            g_LifetimeCurveTimes,
            g_LifetimeCurveTimesBlock1,
            g_LifetimeCurveValues,
            g_LifetimeCurveValuesBlock1,
            g_LifetimeCurveArriveTangents,
            g_LifetimeCurveArriveTangentsBlock1,
            g_LifetimeCurveLeaveTangents,
            g_LifetimeCurveLeaveTangentsBlock1,
            g_LifetimeCurveModes,
            g_LifetimeCurveModesBlock1,
            lifeMax
        );
    }

    return max(0.0001f, result);
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
            lifecycle.lifeMax = SampleLifeMax(lifecycle.respawnSeed + g_SpawnSeedSalts.x);
            lifecycle.lifeAge = 0.0f;
            lifecycle.active = 1u;
            lifecycle.placementIndex = spawnCandidate;
            lifecycle.placementCount = max(1u, min(maxActiveCount, g_SpawnRequest));
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

    const uint seed = lifecycle.respawnSeed;
    const float ratio = ((float)index + 0.5f) / max(1.0f, g_InstanceCount);
    const float scaleRatio = frac(ratio * 7.0f);
    const float colorRatio = frac(ratio * 11.0f);
    const float sizeRatio = g_AppearanceSeedSalts0.x != 0u ? Hash01(seed + g_AppearanceSeedSalts0.x + 233u) : scaleRatio;
    const float initialColorRatio = g_AppearanceSeedSalts0.y != 0u ? Hash01(seed + g_AppearanceSeedSalts0.y + 347u) : colorRatio;
    const float initialAlphaRatio = g_AppearanceSeedSalts0.z != 0u ? Hash01(seed + g_AppearanceSeedSalts0.z + 359u) : colorRatio;
    const float colorOverLifeRatio = g_AppearanceSeedSalts0.w != 0u ? Hash01(seed + g_AppearanceSeedSalts0.w + 367u) : colorRatio;
    const float alphaOverLifeRatio = g_AppearanceSeedSalts1.x != 0u ? Hash01(seed + g_AppearanceSeedSalts1.x + 373u) : colorRatio;
    const float2 baseSize = lerp(g_SizeMin.xy, g_SizeMax.xy, sizeRatio);

    const float lifeMax = max(lifecycle.lifeMax, 0.0001f);
    const float lifeAge = max(0.0f, lifecycle.lifeAge);
    const float lifeProgress = saturate(lifeAge / lifeMax);
    const float2 sizeByLifeMultiplier = EvaluateSizeByLifeMultiplier(lifeProgress);
    const float sizeX = baseSize.x * sizeByLifeMultiplier.x;
    const float sizeY = baseSize.y * sizeByLifeMultiplier.y;

    // particle index에서 만든 deterministic seed다.
    // 같은 particle 생애 안에서는 같은 seed를 사용하므로 spawn offset, speed, rotation 랜덤값이 흔들리지 않는다.
    // 뒤의 +101u, +211u 같은 offset은 X/Y/Z나 속성별 샘플이 서로 같은 패턴을 공유하지 않게 분리하기 위한 값이다.
    const uint initialLocationSeed = seed + g_SpawnSeedSalts.y;
    const uint sphereLocationSeed = seed + g_SpawnSeedSalts.z;
    const uint planeRadialLocationSeed = seed;
    const uint initialVelocitySeed = seed + g_MotionSeedSalts0.x;
    const uint initialRadialVelocitySeed = seed + g_MotionSeedSalts0.y;
    const uint velocityConeSeed = seed + g_MotionSeedSalts0.z;
    const uint accelerationSeed = seed + g_MotionSeedSalts0.w;
    const uint dragSeed = seed + g_MotionSeedSalts1.x;
    const uint initialRotationSeed = seed + g_RotationSeedSalts.x;
    const uint initialRotationRateSeed = seed + g_RotationSeedSalts.y;
    const uint subUVRandomFrameSeed = seed + g_SubUVSeedSalts.x;

    const float3 locationRandom = float3(Hash01(initialLocationSeed), Hash01(initialLocationSeed + 101u), Hash01(initialLocationSeed + 211u));
    const float3 fallbackSpawnOffset = ResolveEmitterBasisVector((locationRandom * 2.0f - 1.0f) * g_SpawnRange.xyz * 0.5f);
    const float3 initialLocationOffset = ResolveEmitterBasisVector(lerp(g_InitialLocationMin.xyz, g_InitialLocationMax.xyz, locationRandom));
    PlaneRadialFrame planeRadialFrame;
    SamplePlaneRadialFrame(
        planeRadialLocationSeed,
        index,
        lifecycle.placementIndex,
        lifecycle.placementCount,
        planeRadialFrame
    );
    const bool hasExplicitLocation = g_InitialLocationMin.w != 0.0f ||
                                     g_SphereLocationParams.x != 0.0f ||
                                     planeRadialFrame.enabled != 0.0f;
    const float3 baseLocationOffset = g_InitialLocationMin.w != 0.0f ? initialLocationOffset : float3(0.0f, 0.0f, 0.0f);
    const float3 planeRadialOffset = planeRadialFrame.enabled != 0.0f
                                     ? ResolveEmitterBasisVector(planeRadialFrame.localOffset)
                                     : float3(0.0f, 0.0f, 0.0f);
    const float3 spawnOffset = (hasExplicitLocation ? baseLocationOffset : fallbackSpawnOffset) +
                               SampleSphereLocationOffset(sphereLocationSeed, lifecycle.placementIndex, lifecycle.placementCount) +
                               planeRadialOffset;
    const float3 orbitSpawnOffset = ApplyOrbitOverLife(spawnOffset, lifeProgress);
    const float3 spawnPosition = g_Center.xyz + orbitSpawnOffset;
    float3 initialVelocity = float3(0.0f, 0.0f, 0.0f);

    if (g_MotionFlags.y != 0.0f)
    {
        const float3 sampledVelocity = SampleRange3(g_InitialVelocityMin.xyz, g_InitialVelocityMax.xyz, initialVelocitySeed + 503u);
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
                Hash01(initialRadialVelocitySeed + 307u) * 2.0f - 1.0f,
                0.25f,
                Hash01(initialRadialVelocitySeed + 401u) * 2.0f - 1.0f
            )
        );
        float3 centerRadialDirection = fallbackRadialDirection;
        if ((uint)g_MotionSpaceFlags.w == 1u && planeRadialFrame.enabled != 0.0f)
            centerRadialDirection = ResolveEmitterBasisVector(planeRadialFrame.radialDirection);
        const float centerRadialLength = length(centerRadialDirection);
        const float3 normalizedCenterRadialDirection = centerRadialLength < 0.0001f
                                                       ? float3(0.0f, 1.0f, 0.0f)
                                                       : centerRadialDirection / centerRadialLength;
        const float radialDirectionLength = length(radialDirection);
        radialDirection = radialDirectionLength < 0.0001f
                          ? normalizedCenterRadialDirection
                          : radialDirection / radialDirectionLength;
        const float radialSpeed = lerp(g_MotionRadialSpeedDrag.x, g_MotionRadialSpeedDrag.y, Hash01(initialRadialVelocitySeed + 509u));
        initialVelocity += radialDirection * radialSpeed;
    }

    if (g_MotionSpaceFlags.y != 0.0f)
    {
        const float3 sampledConeDirection = SampleConeDirection(g_VelocityConeAxisAngle.xyz, g_VelocityConeAxisAngle.w, velocityConeSeed);
        const float3 coneDirection = g_MotionSpaceFlags.z != 0.0f
                                     ? sampledConeDirection
                                     : ResolveEmitterBasisVector(sampledConeDirection);
        const float coneSpeed = lerp(g_VelocityConeSpeed.x, g_VelocityConeSpeed.y, Hash01(velocityConeSeed + 1747u));
        initialVelocity += coneDirection * coneSpeed;
    }

    const float drag = max(0.0f, lerp(g_MotionRadialSpeedDrag.z, g_MotionRadialSpeedDrag.w, Hash01(dragSeed + 607u)));
    const float velocityScale = EvaluateVelocityScaleByLifeAverage(lifeProgress);
    const float dragTime = drag > 0.0001f ? (1.0f - exp(-drag * lifeAge)) / drag : lifeAge;
    const float dragDecay = drag > 0.0001f ? exp(-drag * lifeAge) : 1.0f;
    const float3 acceleration = SampleRange3(g_AccelerationMin.xyz, g_AccelerationMax.xyz, accelerationSeed + 701u);
    float3 accelerationVelocity = acceleration * lifeAge;
    float3 accelerationOffset = acceleration * 0.5f * lifeAge * lifeAge;
    if (g_AccelerationCurveParams.y != 0.0f)
    {
        float3 accelerationVelocityProgress = float3(0.0f, 0.0f, 0.0f);
        float3 accelerationOffsetProgress = float3(0.0f, 0.0f, 0.0f);
        IntegrateAccelerationCurve(lifeProgress, accelerationVelocityProgress, accelerationOffsetProgress);
        accelerationVelocity = accelerationVelocityProgress * lifeMax;
        accelerationOffset = accelerationOffsetProgress * lifeMax * lifeMax;
    }

    const float3 motionOffset = initialVelocity * velocityScale * dragTime + accelerationOffset;
    const float currentVelocityScale = EvaluateVelocityScaleByLife(lifeProgress);
    const float3 currentVelocity = initialVelocity * currentVelocityScale * dragDecay + accelerationVelocity;
    const float currentVelocityLength = length(currentVelocity);
    const float3 velocityAlignmentHint = currentVelocityLength < 0.0001f
                                         ? float3(0.0f, 0.0f, 0.0f)
                                         : currentVelocity / currentVelocityLength;

    const float initialRotation = radians(lerp(g_RotationInitialRate.x, g_RotationInitialRate.y, Hash01(initialRotationSeed + 809u))) +
                                  ResolvePlaneRadialSpriteRotation(planeRadialFrame.enabled, planeRadialFrame.sampleAngle);
    const float rotationOverLife = radians(EvaluateRotationOverLifeDegrees(lifeProgress));
    const float rotationRate = radians(lerp(g_RotationInitialRate.z, g_RotationInitialRate.w, Hash01(initialRotationRateSeed + 907u)));
    const float rotationRateScale = EvaluateRotationRateScaleByLifeAverage(lifeProgress);
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
                (uint)floor(Hash01(subUVRandomFrameSeed + 1201u) * (float)rangeCount),
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
    const float4 legacyStartColor = float4(
        lerp(g_StartColorMin.rgb, g_StartColorMax.rgb, initialColorRatio),
        lerp(g_StartColorMin.a, g_StartColorMax.a, initialAlphaRatio)
    );
    const float4 legacyEndColor = float4(
        lerp(g_EndColorMin.rgb, g_EndColorMax.rgb, colorOverLifeRatio),
        lerp(g_EndColorMin.a, g_EndColorMax.a, alphaOverLifeRatio)
    );
    const float4 particleColor = EvaluateColorOverLife(lifeProgress, lerp(legacyStartColor, legacyEndColor, lifeProgress));
    instance.startColor = particleColor;
    instance.endColor = particleColor;
    instance.subUVRect = float4(u0, v0, u0 + invCols, v0 + invRows);

    if (index >= stableDrawCount)
    {
        LifecycleState[index] = lifecycle;
        return;
    }

    OutputData[index] = instance;
    LifecycleState[index] = lifecycle;
}
