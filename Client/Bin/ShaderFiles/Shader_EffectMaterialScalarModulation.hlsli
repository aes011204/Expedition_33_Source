static const int EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY = 0;
static const int EFFECT_MATERIAL_SCALAR_TARGET_OPACITY_POWER = 1;
static const int EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH = 2;
static const int EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION = 3;
static const int EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF = 4;
static const int EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY = 5;
static const int EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY = 6;
static const int EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR = 7;

static const int EFFECT_MATERIAL_SCALAR_TIME_PARTICLE_LIFE = 0;
static const int EFFECT_MATERIAL_SCALAR_TIME_EMITTER_TIME = 1;

// float4에 패킹된 키 데이터에서 지정한 채널 값을 꺼낸다.
float EffectMaterialScalar_ReadComponent(float4 values, uint index)
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

// 모듈레이터 한 개의 커브를 현재 phase 기준으로 평가한다.
float EffectMaterialScalar_EvaluateCurve(uint modulatorIndex, float phase)
{
    float result = 1.f;
    const uint keyCount = min(4u, (uint)g_EffectMaterialScalarModulationMeta[modulatorIndex].w);
    if (keyCount > 0u)
    {
        result = EffectMaterialScalar_ReadComponent(g_EffectMaterialScalarModulationKeyValues[modulatorIndex], 0u);
        if (keyCount > 1u)
        {
            const float clampedPhase = saturate(phase);
            [loop]
            for (uint keyIndex = 1u; keyIndex < 4u; ++keyIndex)
            {
                if (keyIndex >= keyCount)
                    break;

                const float rightTime = EffectMaterialScalar_ReadComponent(g_EffectMaterialScalarModulationKeyTimes[modulatorIndex], keyIndex);
                if (clampedPhase > rightTime)
                {
                    result = EffectMaterialScalar_ReadComponent(g_EffectMaterialScalarModulationKeyValues[modulatorIndex], keyIndex);
                    continue;
                }

                const uint leftIndex = keyIndex - 1u;
                const float leftTime = EffectMaterialScalar_ReadComponent(g_EffectMaterialScalarModulationKeyTimes[modulatorIndex], leftIndex);
                const float leftValue = EffectMaterialScalar_ReadComponent(g_EffectMaterialScalarModulationKeyValues[modulatorIndex], leftIndex);
                const float rightValue = EffectMaterialScalar_ReadComponent(g_EffectMaterialScalarModulationKeyValues[modulatorIndex], keyIndex);
                const float width = max(0.0001f, rightTime - leftTime);
                const float ratio = saturate((clampedPhase - leftTime) / width);
                const int mode = (int)EffectMaterialScalar_ReadComponent(g_EffectMaterialScalarModulationKeyModes[modulatorIndex], leftIndex);

                if (mode == 0)
                {
                    result = leftValue;
                }
                else if (mode == 2)
                {
                    const float leftLeave = EffectMaterialScalar_ReadComponent(g_EffectMaterialScalarModulationKeyLeaveTangents[modulatorIndex], leftIndex) * width;
                    const float rightArrive = EffectMaterialScalar_ReadComponent(g_EffectMaterialScalarModulationKeyArriveTangents[modulatorIndex], keyIndex) * width;
                    const float t2 = ratio * ratio;
                    const float t3 = t2 * ratio;
                    result = (2.f * t3 - 3.f * t2 + 1.f) * leftValue
                        + (t3 - 2.f * t2 + ratio) * leftLeave
                        + (-2.f * t3 + 3.f * t2) * rightValue
                        + (t3 - t2) * rightArrive;
                }
                else
                {
                    result = lerp(leftValue, rightValue, ratio);
                }
                break;
            }
        }
    }

    return result;
}

// 대상 필드에 연결된 모든 모듈레이터를 곱해 최종 배율을 만든다.
float EffectMaterialScalar_ResolveMultiplier(int targetField, float lifeProgress)
{
    float multiplier = 1.f;
    const uint count = min(8u, (uint)g_EffectMaterialScalarModulationParams.x);

    [loop]
    for (uint index = 0u; index < 8u; ++index)
    {
        if (index >= count)
            break;

        const float4 meta = g_EffectMaterialScalarModulationMeta[index];
        if (meta.x < 0.5f || (int)meta.y != targetField)
            continue;

        const int timeSource = (int)meta.z;
        const float phase = timeSource == EFFECT_MATERIAL_SCALAR_TIME_EMITTER_TIME
            ? g_EffectMaterialScalarModulationParams.y
            : lifeProgress;
        multiplier *= EffectMaterialScalar_EvaluateCurve(index, phase);
    }

    return multiplier;
}
