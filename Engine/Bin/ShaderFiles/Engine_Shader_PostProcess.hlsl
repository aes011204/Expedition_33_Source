// Engine 원본 경로: Engine/Bin/ShaderFiles/Engine_Shader_PostProcess.hlsl
// Client 복사본 경로: Client/Bin/ShaderFiles/Engine_Shader_PostProcess.hlsl - 먼저 이 원본의 의도를 확인하세요.
#ifndef ENGINE_SHADER_POSTPROCESS_HLSL
#define ENGINE_SHADER_POSTPROCESS_HLSL

#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"

//==================================================
// MATERIAL TEXTURES
//==================================================
Texture2D g_HDRTexture;
Texture2D g_DepthTexture;
Texture2D g_BloomTexture;
Texture2D g_BloomBaseTexture;
Texture2D g_BackgroundMaskTexture;
Texture2D g_DistortionSourceTexture;
Texture2D g_DistortionFieldTexture;
Texture2D g_LightShaftMaskTexture;
Texture2D g_LightShaftSourceTexture;
Texture2D g_DOFCoCTexture;
Texture2D g_DOFSourceTexture;
Texture2D g_DOFNearTexture;
Texture2D g_DOFFarTexture;
Texture3D g_FroxelIntegratedTexture;

// Bloom Extract 용도
static const float3 REC_709 = float3(0.2126f, 0.7152f, 0.0722f);
static const float2 HORIZONTAL = float2(1.f, 0.f);
static const float2 VERTICAL = float2(0.f, 1.f);

//==================================================
// 컴파일 단계에서 inline 최적화 진행됨
//==================================================
bool HasPostProcessFeature(uint feature)
{
    return (g_PP_FeatureMask & feature) != 0;
}

//==================================================
// SAMPLE FUNCTIONS
//==================================================
float3 SampleGaussianDownsample_4tap(float2 uv)
{
    float2 texel = max(g_TexelSize, float2(EPSILON, EPSILON));
    float2 offset = texel * 1.5f;

    const float w0 = 0.25f;
    const float w1 = 0.25f;
    const float w2 = 0.25f;
    const float w3 = 0.25f;

    float3 c0 = g_BloomTexture.Sample(LinearClampSampler, uv + float2(-offset.x, -offset.y)).rgb;
    float3 c1 = g_BloomTexture.Sample(LinearClampSampler, uv + float2(offset.x, -offset.y)).rgb;
    float3 c2 = g_BloomTexture.Sample(LinearClampSampler, uv + float2(-offset.x, offset.y)).rgb;
    float3 c3 = g_BloomTexture.Sample(LinearClampSampler, uv + float2(offset.x, offset.y)).rgb;

    return c0 * w0 + c1 * w1 + c2 * w2 + c3 * w3;
}

float3 SampleGaussianBlur_5tap(float2 uv, float2 dir)
{
    float2 texel = g_TexelSize * max(EPSILON, g_PP_BloomRadius);

    const float w0 = 0.227027f;
    const float w1 = 0.316216f;
    const float w2 = 0.070270f;

    float3 c = g_BloomTexture.Sample(LinearClampSampler, uv).rgb * w0;

    c += g_BloomTexture.Sample(LinearClampSampler, uv + texel * 1.f * dir).rgb * w1;
    c += g_BloomTexture.Sample(LinearClampSampler, uv - texel * 1.f * dir).rgb * w1;

    c += g_BloomTexture.Sample(LinearClampSampler, uv + texel * 2.f * dir).rgb * w2;
    c += g_BloomTexture.Sample(LinearClampSampler, uv - texel * 2.f * dir).rgb * w2;

    return min(c, g_PP_BloomClamp);
}

float3 SampleBloomUpsampleAdd(float2 uv)
{
    float3 upsampled = g_BloomTexture.Sample(LinearClampSampler, uv).rgb;
    float3 baseColor = g_BloomBaseTexture.Sample(LinearClampSampler, uv).rgb;
    return min(baseColor + upsampled, g_PP_BloomClamp);
}

float GetDOFViewZ(float2 uv)
{
    const float viewZ = g_DepthTexture.Sample(PointClampSampler, uv).y * g_DeferredFar;
    return (viewZ <= EPSILON) ? g_DeferredFar : viewZ;
}

float2 ComputeDOFCoC(float viewZ)
{
    if (viewZ <= EPSILON)
        return float2(0.f, 1.f);

    const float focusDistance = max(g_PP_DOFFocusDistance, EPSILON);
    const float focusRange = max(g_PP_DOFFocusRange, EPSILON);
    const float nearStart = max(focusDistance - focusRange, EPSILON);
    const float farStart = focusDistance + focusRange;

    const float nearCoC = saturate((nearStart - viewZ) / max(focusRange, EPSILON));
    const float farCoC = saturate((viewZ - farStart) / max(focusRange, EPSILON));

    return float2(nearCoC, farCoC);
}

float ApplyDOFBokeh(float3 color)
{
    const float luminance = dot(color, REC_709);
    return saturate((luminance - g_PP_DOFBokehThreshold) * g_PP_DOFBokehIntensity);
}

float ComputeDOFBlendSoftness(float coc)
{
    const float softened = saturate(coc);
    return softened * softened * (3.f - 2.f * softened);
}

static const float2 DOFPoissonDisk[12] =
{
    float2(0.000f, 0.000f),
    float2(0.527f, 0.086f),
    float2(-0.416f, 0.311f),
    float2(0.178f, -0.497f),
    float2(-0.660f, -0.190f),
    float2(0.771f, -0.379f),
    float2(-0.219f, 0.753f),
    float2(0.458f, 0.633f),
    float2(-0.858f, 0.349f),
    float2(0.057f, -0.889f),
    float2(0.930f, 0.203f),
    float2(-0.565f, -0.704f)
};

float4 SampleDOFBlur(float2 uv, bool nearBlur)
{
    const float2 coc = g_DOFCoCTexture.Sample(LinearClampSampler, uv).rg;
    const float centerCoC = nearBlur ? coc.r : coc.g;

    if (centerCoC <= g_PP_DOFCoCEpsilon)
        return float4(0.f, 0.f, 0.f, 0.f);

    const float centerDepth = GetDOFViewZ(uv);
    const float maxRadius = nearBlur ? g_PP_DOFNearMaxRadius : g_PP_DOFFarMaxRadius;
    const float radius = lerp(maxRadius * 0.35f, maxRadius, sqrt(saturate(centerCoC)));
    const uint sampleCount = min(max(g_PP_DOFSampleCount, 1u), 12u);
    const float2 texel = max(g_TexelSize, float2(EPSILON, EPSILON));

    float3 accum = float3(0.f, 0.f, 0.f);
    float coverageAccum = 0.f;
    float weightSum = 0.f;

    [loop]
    for (uint i = 0u; i < sampleCount; ++i)
    {
        const float2 sampleUV = saturate(uv + DOFPoissonDisk[i] * texel * radius);
        const float4 sampleBlur = g_DOFSourceTexture.Sample(LinearClampSampler, sampleUV);
        const float2 sampleCoC = g_DOFCoCTexture.Sample(LinearClampSampler, sampleUV).rg;
        const float sampleWeightCoC = nearBlur ? max(sampleBlur.a, centerCoC) : max(sampleCoC.g, centerCoC);

        if (sampleWeightCoC <= g_PP_DOFCoCEpsilon)
            continue;

        const float sampleDepth = GetDOFViewZ(sampleUV);
        float depthWeight = 1.f;
        const float sharpness = max(g_PP_DOFDepthSharpness, EPSILON);

        if (nearBlur)
        {
            const float backgroundDelta = sampleDepth - centerDepth;
            depthWeight = (backgroundDelta > 0.f)
                ? exp(-backgroundDelta * sharpness * 0.15f)
                : 1.f;
        }
        else
        {
            const float foregroundDelta = max(centerDepth - sampleDepth, 0.f);
            const float relativeForegroundDelta = foregroundDelta / max(centerDepth, 1.f);
            depthWeight = exp(-relativeForegroundDelta / sharpness);
        }

        const float cocSoftness = lerp(0.35f, 1.f, ComputeDOFBlendSoftness(sampleWeightCoC));
        const float weight = sampleWeightCoC * depthWeight * cocSoftness;
        float3 sampleColor = sampleBlur.rgb;

        const float bokeh = ApplyDOFBokeh(sampleColor);
        sampleColor *= 1.f + bokeh * sampleWeightCoC;

        accum += sampleColor * weight;
        coverageAccum += sampleWeightCoC * weight;
        weightSum += weight;
    }

    if (weightSum <= EPSILON)
        return float4(0.f, 0.f, 0.f, 0.f);

    const float3 blurColor = accum / weightSum;
    const float blurCoverage = nearBlur ? saturate(max(centerCoC, coverageAccum / weightSum)) : saturate(centerCoC);

    return float4(blurColor, blurCoverage);
}

// god ray가 화면 가장자리에서 갑자기 켜지거나 꺼지는 popping을 완화.
float ComputeLightShaftScreenFade(float2 lightScreenUV)
{
    const float edgeFade = max(g_LS_EdgeFade, EPSILON);
    const float2 minFade = saturate((lightScreenUV + edgeFade) / edgeFade);
    const float2 maxFade = saturate((1.f + edgeFade - lightScreenUV) / edgeFade);
    return min(minFade.x, minFade.y) * min(maxFade.x, maxFade.y);
}

#endif
