#ifndef ENGINE_SHADER_POSTPROCESS_HLSL
#define ENGINE_SHADER_POSTPROCESS_HLSL

#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"

//==================================================
// MATERIAL TEXTURES
//==================================================
Texture2D g_HDRTexture;
Texture2D g_BloomTexture;
Texture2D g_BloomBaseTexture;
Texture2D g_BackgroundMaskTexture;

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

#endif
