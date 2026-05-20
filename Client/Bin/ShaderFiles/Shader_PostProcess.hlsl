#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Utils.hlsl"
#include "Engine_Shader_PostProcess.hlsl"

//==================================================
// VS
//==================================================
struct VS_IN
{
    float3 Position : POSITION;
    float2 Texcoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float2 Texcoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV, matWVP;

    matWV = mul(g_WorldMatrix, g_ScreenViewMatrix);
    matWVP = mul(matWV, g_ScreenProjMatrix);

    Out.Position = mul(float4(In.Position, 1.f), matWVP);
    Out.Texcoord = In.Texcoord;

    return Out;
}

//==================================================
// PS
//==================================================
struct PS_IN
{
    float4 Position : SV_POSITION; 
    float2 Texcoord : TEXCOORD0;
};

struct PS_OUT_COLOR
{
    vector Color : SV_TARGET0;
};

struct PS_OUT_BACKBUFFER
{
    vector Color : SV_TARGET0;
};

PS_OUT_COLOR PS_Debug(PS_IN In)
{
    PS_OUT_COLOR Out;

    Out.Color = float4(0.f, 0.f, 0.f, 1.f);
    return Out;
}

PS_OUT_BACKBUFFER PS_BloomExtractHalf(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float2 texel = max(g_TexelSize, float2(EPSILON, EPSILON));
    float2 offset = texel * 1.5f;

    float3 c0 = g_HDRTexture.Sample(LinearClampSampler, In.Texcoord + float2(-offset.x, -offset.y)).rgb;
    float3 c1 = g_HDRTexture.Sample(LinearClampSampler, In.Texcoord + float2(offset.x, -offset.y)).rgb;
    float3 c2 = g_HDRTexture.Sample(LinearClampSampler, In.Texcoord + float2(-offset.x, offset.y)).rgb;
    float3 c3 = g_HDRTexture.Sample(LinearClampSampler, In.Texcoord + float2(offset.x, offset.y)).rgb;
    float3 color = (c0 + c1 + c2 + c3) * 0.25f;

    // 컬러를 밝기로 표현
    float luminant = dot(color, REC_709);

    // soft knee 기반 가중치
    float threshold = max(0.f, g_PP_BloomThreshold);
    float knee = max(g_PP_BloomSoftKnee, EPSILON);

    // bloom이 갑자기 켜지지 않도록, 제곱 형태로 증가시킨다.
    float x = luminant - threshold;                             // if (x <= 0)          bloomWeight = 0;
    float soft = saturate(x / knee);                            // else if (x < knee)   bloomWeight = x * x / knee;
    float bloomWeight = max(x, 0.f) * lerp(0.2f, 1.f, soft);    // else                 bloomWeight = x;

    float3 bloomColor = color * bloomWeight * g_PP_BloomIntensity;
    bloomColor = min(bloomColor, g_PP_BloomClamp);

    Out.Color = float4(bloomColor, 1.f);
    return Out;
}

PS_OUT_COLOR PS_BloomBlur_H(PS_IN In)
{
    PS_OUT_COLOR Out;

    Out.Color = float4(SampleGaussianBlur_5tap(In.Texcoord, HORIZONTAL), 1.f);
    return Out;
}

PS_OUT_COLOR PS_BloomDownsample(PS_IN In)
{
    PS_OUT_COLOR Out;
    Out.Color = float4(SampleGaussianDownsample_4tap(In.Texcoord), 1.f);
    return Out;
}

PS_OUT_COLOR PS_BloomBlur_V(PS_IN In)
{
    PS_OUT_COLOR Out;

    Out.Color = float4(SampleGaussianBlur_5tap(In.Texcoord, VERTICAL), 1.f);
    return Out;
}

PS_OUT_COLOR PS_BloomUpsampleAdd(PS_IN In)
{
    PS_OUT_COLOR Out;
    Out.Color = float4(SampleBloomUpsampleAdd(In.Texcoord), 1.f);
    return Out;
}

PS_OUT_BACKBUFFER PS_ToneMapping(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float4 hdr = g_HDRTexture.Sample(LinearClampSampler, In.Texcoord);
    float3 bloomColor = 0.f;
    if (HasPostProcessFeature(POSTPROCESS_FLAG_BLOOM))
        bloomColor = g_BloomTexture.Sample(LinearClampSampler, In.Texcoord).rgb;

    float3 color = hdr.rgb + bloomColor * g_PP_BloomIntensity;
    
    // Exposure
    if (HasPostProcessFeature(POSTPROCESS_FLAG_EXPOSURE))
        color *= max(g_PP_Exposure, EPSILON);
    
    // ToneMapping
    if (HasPostProcessFeature(POSTPROCESS_FLAG_TONEMAP))
    {
        if (g_PP_ToneMapMode == 0)
        {
            // Reinhard
            color = color / (1.f + color);
        }
        else if (g_PP_ToneMapMode == 1)
        {
            // ACES fitted
            const float a = 2.51f;
            const float b = 0.03f;
            const float c = 2.43f;
            const float d = 0.59f;
            const float e = 0.14f;
            color = saturate((color * (a * color + b)) / (color * (c * color + d) + e));
        }
        else if (g_PP_ToneMapMode == 2)
        {
            // Uncharted2 filmic
            const float A = 0.15f;
            const float B = 0.50f;
            const float C = 0.10f;
            const float D = 0.20f;
            const float E = 0.02f;
            const float F = 0.30f;
            const float W = max(1.f / max(g_PP_InvWhitePoint, EPSILON), EPSILON);

            float3 numerator = color * (A * color + C * B) + D * E;
            float3 denominator = color * (A * color + B) + D * F;
            float3 mapped = numerator / max(denominator, EPSILON.xxx) - E / F;

            float whiteNumerator = W * (A * W + C * B) + D * E;
            float whiteDenominator = W * (A * W + B) + D * F;
            float whiteScale = whiteNumerator / max(whiteDenominator, EPSILON) - E / F;

            color = mapped / max(whiteScale, EPSILON);
        }
    }
    
    // Gamma
    if (HasPostProcessFeature(POSTPROCESS_FLAG_GAMMA))
    {
        float gamma = max(g_PP_Gamma, EPSILON);
        color = pow(saturate(color), 1.f / gamma);
    }
    // 최종 출력 화면은 LDR
    Out.Color = float4(saturate(color), hdr.a);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(HDRDebug,          RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_ToneMapping)             // 0
    PASS_RS_DS_BS_VP(BloomExtractHalf,  RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_BloomExtractHalf)        // 1
    PASS_RS_DS_BS_VP(BloomDownsample,   RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_BloomDownsample)         // 2
    PASS_RS_DS_BS_VP(BloomBlurH,        RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_BloomBlur_H)             // 3
    PASS_RS_DS_BS_VP(BloomBlurV,        RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_BloomBlur_V)             // 4
    PASS_RS_DS_BS_VP(BloomUpsampleAdd,  RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_BloomUpsampleAdd)        // 5
    PASS_RS_DS_BS_VP(ToneMapping,       RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_ToneMapping)             // 6
}
