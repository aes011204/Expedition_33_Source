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

float3 SampleGaussianBlur_5tap(float2 uv, float2 dir)
{
    float2 texel = g_PP_BloomTexelSize * max(EPSILON, g_PP_BloomRadius);

    const float w0 = 0.227027f;
    const float w1 = 0.316216f;
    const float w2 = 0.070270f;

    float c = g_BloomTexture.Sample(LinearClampSampler, uv).rgb * w0;
    c += g_BloomTexture.Sample(LinearClampSampler, uv + texel * 1.f * dir).rgb * w1;
    c += g_BloomTexture.Sample(LinearClampSampler, uv + texel * 1.f * dir).rgb * w2;
    c += g_BloomTexture.Sample(LinearClampSampler, uv - texel * 2.f * dir).rgb * w1;
    c += g_BloomTexture.Sample(LinearClampSampler, uv - texel * 2.f * dir).rgb * w2;

    return min(c, g_PP_BloomClamp);
}

#endif
