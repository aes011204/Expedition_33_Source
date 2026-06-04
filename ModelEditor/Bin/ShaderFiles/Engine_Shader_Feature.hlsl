#ifndef ENGINE_SHADER_FEATURE_HLSLI
#define ENGINE_SHADER_FEATURE_HLSLI

#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"

// DrawFeature 전용 텍스처 배열
Texture2D g_DrawFeatureTextures[FEATURE_TEX_END];

// 해당 feature을 정말 실행해도 되는지 active & support 둘 다 체크
bool HasDrawFeature(uint featureFlag)
{
    const uint activeMask = g_ActiveFeatureMask & featureFlag;
    const uint supportMask = g_SupportFeatureMask & featureFlag;
    return activeMask != 0u && supportMask != 0u;
}

// 해당 Feature Texture 슬롯에 텍스처가 실제로 들어와 있는지 검사
bool HasFeatureTexture(uint iSlot)
{
    if (iSlot >= FEATURE_TEX_END)
        return false;

    return (g_FeatureTextureMask & (1u << iSlot)) != 0u;
}

// Feature Texture에서 .r 채널을 샘플링하는 헬퍼 함수.
float SampleFeatureNoise(float2 uv, uint iSlot)
{
    if (!HasFeatureTexture(iSlot))
        return 1.f;

    return g_DrawFeatureTextures[iSlot].Sample(LinearWrapSampler, uv).r;
}

// Feature Texture에서 .r 채널을 샘플링하는 헬퍼 함수. (마스크 용도)
float SampleFeatureMask(float2 uv, uint iSlot)
{
    if (!HasFeatureTexture(iSlot))
        return 1.f;

    return g_DrawFeatureTextures[iSlot].Sample(LinearWrapSampler, uv).r;
}

// 플레이어 간 switch 시 사용하는 dissolve 
bool IsSwitchDissolveEnabled()
{
    return HasDrawFeature(FEATURE_FLAG_DISSOLVE) &&
           HasFeatureTexture(FEATURE_TEX_DISSOLVE_NOISE);
}

// UI Preview에서 역광 효과가 활성화되었는지 확인
bool HasPreviewRim()
{
    return HasDrawFeature(FEATURE_FLAG_PREVIEW_RIM);
}

// UI Preview에서 옷 전체가 번들거리지 않게 외곽 위주로만 림을 잡는다. 
float ComputePreviewRim(float3 worldNormal, float3 viewDir, float rimPower)
{
    const float safePower = max(rimPower, 0.0001f);
    const float ndotv = saturate(dot(normalize(worldNormal), normalize(viewDir)));
    const float rimBase = 1.f - ndotv;
    const float edgeOnly = smoothstep(0.62f, 0.90f, rimBase);
    return pow(edgeOnly, safePower);
}

// UI Preview에서 역광 효과 색상과 강도 가져오는 함수
float3 GetPreviewRimColor()
{
    return g_FeatureParam2.rgb;
}

// UI Preview에서 역광 효과 강도 가져오는 함수
float GetPreviewRimIntensity()
{
    return g_FeatureParam2.a;
}

// UI Preview에서 머리카락 역광 효과 강도 가져오는 함수
float GetPreviewHairRimIntensity()
{
    return g_FeatureParam3.y;
}

float SampleSwitchDissolveNoise(float2 uv)
{
    // noiseScale이 클수록 노이즈 패턴이 촘촘해진다.
    const float noiseScale = max(g_FeatureParam1.a, EPSILON);
    const float2 scaledUV = uv * noiseScale;

    // 노이즈를 세 번 샘플링해서 painterly한 느낌 : UV 스케일, 오프셋, 방향을 다르게 해서 여러 번 읽고 섞는다.
    const float baseNoise = SampleFeatureNoise(scaledUV, FEATURE_TEX_DISSOLVE_NOISE);
    const float detailNoise =
        SampleFeatureNoise(scaledUV * 1.73f + float2(1.37f, -2.41f), FEATURE_TEX_DISSOLVE_NOISE);
    const float streakNoise =
        SampleFeatureNoise(
            float2(
                scaledUV.x * 0.82f - scaledUV.y * 0.45f,
                scaledUV.x * 0.36f + scaledUV.y * 1.18f) * 1.41f + float2(-3.10f, 0.80f),
            FEATURE_TEX_DISSOLVE_NOISE);

    const float painterlyNoise = baseNoise * 0.52f + detailNoise * 0.30f + streakNoise * 0.18f;
    return saturate(smoothstep(0.10f, 0.92f, painterlyNoise));
}

// dissolve clip 값에 scatter 노이즈를 추가해서 좀 더 자연스럽게 보이도록 한다.
float SampleSwitchDissolveScatter(float2 uv)
{
    const float noiseScale = max(g_FeatureParam1.a, EPSILON);
    const float2 scaledUV = uv * noiseScale;

    const float scatterA =
        SampleFeatureNoise(scaledUV * 2.31f + float2(-1.80f, 2.70f), FEATURE_TEX_DISSOLVE_NOISE);
    const float scatterB =
        SampleFeatureNoise(
            float2(
                scaledUV.x + scaledUV.y * 0.60f,
                scaledUV.y - scaledUV.x * 0.40f) * 1.67f + float2(0.90f, -1.40f),
            FEATURE_TEX_DISSOLVE_NOISE);
    
    // 0 ~ 1 -> -1 ~ 1 범위로 변환
    return (scatterA * 0.65f + scatterB * 0.35f - 0.5f) * 2.f;
}

float ComputeSwitchDissolveClip(float2 uv, float progress)
{
    const float dissolveProgress = saturate(progress);
    const float noise = SampleSwitchDissolveNoise(uv);
    const float scatter = SampleSwitchDissolveScatter(uv);
    const float revealMode = g_FeatureParam0.w;

    // noise < threshold 이면 clip 된다. 
    // reveal : threshold가 점점 줄어들면서 살아남는 픽셀을 늘려야 함
    const float threshold = (revealMode >= 0.5f) ? (1.f - dissolveProgress) : dissolveProgress;
    const float scatterWeight = lerp(0.13f, 0.04f, dissolveProgress);
    const float bandLift = (1.f - dissolveProgress) * 0.05f; 

    return noise - threshold + scatter * scatterWeight + bandLift;
}

// clip 경계 근처에만 0~1짜리 edge 값을 만든다.    
float ComputeSwitchDissolveEdge(float2 uv, float clipValue)
{
    const float edgeWidth = max(g_FeatureParam0.y, 0.0001f);
    const float edgeSoftness = max(g_FeatureParam0.z, 0.f);
    const float edgeOuter = edgeWidth * 2.8f + edgeSoftness * 1.9f + 0.02f;
    const float edgeInner = max(edgeWidth * 0.12f, 0.0001f);
    const float softEntry = -max(edgeSoftness * 1.6f, 0.02f);

    // wideBand로 띠 형태 생성하기 
    const float wideBand = 1.f - smoothstep(softEntry, edgeOuter, clipValue);
    const float innerCut = smoothstep(-edgeInner, edgeWidth * 0.65f + edgeSoftness * 0.45f, clipValue);
    const float painterNoise = SampleSwitchDissolveNoise(uv * 0.85f + float2(0.13f, -0.09f));
    const float scatterNoise = saturate(0.5f + 0.5f * SampleSwitchDissolveScatter(uv * 1.12f));
    const float paintMask = lerp(0.72f, 1.18f, painterNoise) * lerp(0.82f, 1.12f, scatterNoise);

    const float edge = saturate((wideBand - innerCut * 0.40f) * paintMask);
    return saturate(pow(edge, 0.85f) * 0.92f);
}

float ApplySwitchDissolveClip(float2 uv)
{
    const float clipValue = ComputeSwitchDissolveClip(uv, g_FeatureParam0.x);
    clip(clipValue);
    return ComputeSwitchDissolveEdge(uv, clipValue);
}

#endif
