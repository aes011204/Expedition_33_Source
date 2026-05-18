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

PS_OUT_BACKBUFFER PS_BloomExtract(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float3 color = g_HDRTexture.Sample(LinearClampSampler, In.Texcoord).rgb;

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

PS_OUT_BACKBUFFER PS_ToneMapping(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float4 hdr = g_HDRTexture.Sample(LinearClampSampler, In.Texcoord);
    float backgroundMask = g_BackgroundMaskTexture.Sample(PointClampSampler, In.Texcoord).r;
    float3 bloomColor = 0.f;
    if (HasPostProcessFeature(POSTPROCESS_FLAG_BLOOM))
        bloomColor = g_BloomTexture.Sample(LinearClampSampler, In.Texcoord).rgb;

    float3 color = hdr.rgb + bloomColor;
    // Exposure
    if (HasPostProcessFeature(POSTPROCESS_FLAG_EXPOSURE))
        color *= max(g_PP_Exposure, EPSILON);
    
    // ToneMapping  현재는 Reinhard
    if (HasPostProcessFeature(POSTPROCESS_FLAG_TONEMAP))
        color = color / (1.f + color);          
    
    // Gamma
    if (HasPostProcessFeature(POSTPROCESS_FLAG_GAMMA))
    {
        float gamma = max(g_PP_Gamma, EPSILON);
        color = pow(saturate(color), 1.f / gamma);
    }

    float outAlpha = hdr.a;
    // 마스크가 비어있고(HDR 미기여) HDR 밝기 합도 거의 0이면 배경으로 간주한다.
    const bool hasSceneContribution = (backgroundMask > 0.5f) || (dot(abs(hdr.rgb), float3(1.f, 1.f, 1.f)) > 0.0001f);
    if (g_PP_UseBackgroundFallback != 0 && !hasSceneContribution)
    {
        color = g_PP_BackgroundFallbackColor.rgb;
        outAlpha = 1.f;
    }

    // 최종 출력 화면은 LDR
    Out.Color = float4(saturate(color), outAlpha);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(HDRDebug,          RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_ToneMapping)             // 0
    PASS_RS_DS_BS_VP(BloomExtract,      RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_BloomExtract)            // 1
    PASS_RS_DS_BS_VP(BloomDownsample,   RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_BloomDownsample)         // 2
    PASS_RS_DS_BS_VP(BloomBlurH,        RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_BloomBlur_H)             // 3
    PASS_RS_DS_BS_VP(BloomBlurV,        RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_BloomBlur_V)             // 4
    PASS_RS_DS_BS_VP(ToneMapping,       RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_ToneMapping)             // 5
}
