#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Utils.hlsl"
#include "Engine_Shader_PostProcess.hlsl"
#include "Shader_EffectDistortionCommon.hlsli"

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

struct PS_OUT_DOF_PREFILTER
{
    vector CoC      : SV_TARGET0;
    vector NearBlur : SV_TARGET1;
    vector FarBlur  : SV_TARGET2;
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

    float3 bloomColor = color * bloomWeight;
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

// 해당 픽셀을 God Ray 계산에 포함시킬지 정하는 마스크 맵
PS_OUT_COLOR PS_LightShaftMaskHalf(PS_IN In)
{
    PS_OUT_COLOR Out;

    const float active = g_LS_LightVisible;
    if (active <= EPSILON)
    {
        Out.Color = float4(0.f, 0.f, 0.f, 1.f);
        return Out;
    }

    // 화면 상 광원의 위치
    const float2 lightUV = g_LS_LightScreenUV;
    // 그 위치가 화면 경계에 가까울수록 fade 시켜서 popping 완화 (가중치)
    const float screenFade = ComputeLightShaftScreenFade(lightUV);
    if (screenFade <= EPSILON)
    {
        Out.Color = float4(0.f, 0.f, 0.f, 1.f);
        return Out;
    }

    // 태양 중심으로 그라데이션 원 만들기
    const float2 delta = In.Texcoord - lightUV;
    const float dist = length(delta);
    const float sunDisc = saturate(1.f - dist / max(g_LS_SunDiscRadius, EPSILON));

    // 깊이값이 일정 이하인 부분은 빛이 통과하는 빈 공간으로 간주해서 마스크를 씌움
    const float4 depthSample = g_DepthTexture.Sample(LinearClampSampler, In.Texcoord);  // 정규화된 뷰 스페이스의 깊이
    const float empty = 1.f - smoothstep(0.f, max(g_LS_DepthEmptyThreshold, EPSILON), depthSample.y);
    
    // 태양 중심 근처의 강한 마스크. God Ray가 화면에 퍼질 수 있는 범위, 세기 마스크
    const float mask = empty * sunDisc * sunDisc * screenFade * active;
    Out.Color = float4(mask, 0.f, 0.f, 1.f);
    return Out;
}

PS_OUT_COLOR PS_LightShaftRadialBlurHalf(PS_IN In)
{
    PS_OUT_COLOR Out;

    const float active = g_LS_LightVisible;
    if (active <= EPSILON || g_LS_Intensity <= EPSILON)
    {
        Out.Color = float4(0.f, 0.f, 0.f, 1.f);
        return Out;
    }

    const uint sampleCount = max(g_LS_SampleCount, 1u);
    const float2 center = g_LS_LightScreenUV;
    // 태양 중심에서 현재 픽셀까지의 방향으로 일정 간격(sampleCount 기반)만큼 샘플링해서 누적하는 방식
    const float2 stepUV = ((In.Texcoord - center) * g_LS_RayLength) / max((float) sampleCount, 1.f);

    float sampleWeight = 1.f;
    float shaft = 0.f;
    float totalWeight = 0.f;
    float2 sampleUV = In.Texcoord - stepUV * 0.5f;

    // 감쇠하면서 태양 중심으로 퍼지는 형태의 Radial Blur. 샘플링 늘어나면 비용도 늘어남. 
    [loop]
    for (uint i = 0u; i < sampleCount; ++i)
    {
        shaft += g_LightShaftMaskTexture.Sample(LinearClampSampler, sampleUV).r * sampleWeight;
        totalWeight += sampleWeight;
        sampleWeight *= g_LS_Decay;
        sampleUV -= stepUV;
    }

    shaft = (totalWeight > EPSILON) ? (shaft / totalWeight) : 0.f;

    Out.Color = float4(shaft * g_LS_Intensity, 0.f, 0.f, 1.f);
    return Out;
}

PS_OUT_COLOR PS_LightShaftCompositeHDR(PS_IN In)
{
    PS_OUT_COLOR Out;

    const float4 baseColor = g_DistortionSourceTexture.Sample(LinearClampSampler, In.Texcoord);
    const float shaft = g_LightShaftSourceTexture.Sample(LinearClampSampler, In.Texcoord).r;
    const float3 shaftColor = shaft * g_LS_ShaftColor;

    Out.Color = float4(baseColor.rgb + shaftColor, baseColor.a);
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
    Out.Color = float4(saturate(color), 1.f);
    return Out;
}

PS_OUT_COLOR PS_DistortionResolve(PS_IN In)
{
    PS_OUT_COLOR Out;

    const float4 source = g_DistortionSourceTexture.Sample(LinearClampSampler, In.Texcoord);
    const float4 field = g_DistortionFieldTexture.Sample(LinearClampSampler, In.Texcoord);
    // DistortionField: rg = offset * coverage, b = lens coverage, a = reserved.
    const float accumulatedLensCoverage = field.b;
    const float lensCoverage = saturate(accumulatedLensCoverage);

    if (lensCoverage <= EPSILON)
    {
        Out.Color = source;
        return Out;
    }

    const float2 offset = EffectDistortion_ResolveFieldOffset(field.rg, accumulatedLensCoverage);
    const float4 distorted = g_DistortionSourceTexture.Sample(LinearClampSampler, In.Texcoord + offset);
    const float edgeAlpha = EffectDistortion_ComputeLensEdgeAlpha(lensCoverage);

    Out.Color = lerp(source, distorted, edgeAlpha);
    return Out;
}

PS_OUT_DOF_PREFILTER PS_DOFPreFilterHalf(PS_IN In)
{
    PS_OUT_DOF_PREFILTER Out;

    const float2 sourceTexel = max(g_TexelSize * 0.5f, float2(EPSILON, EPSILON));
    const float2 offsets[4] =
    {
        float2(-sourceTexel.x, -sourceTexel.y),
        float2(sourceTexel.x, -sourceTexel.y),
        float2(-sourceTexel.x, sourceTexel.y),
        float2(sourceTexel.x, sourceTexel.y)
    };

    float3 colorAccum = 0.f;
    float weightSum = 0.f;
    float nearMax = 0.f;
    float farMax = 0.f;

    [unroll]
    for (uint i = 0u; i < 4u; ++i)
    {
        const float2 sampleUV = saturate(In.Texcoord + offsets[i]);
        const float4 hdr = g_HDRTexture.Sample(LinearClampSampler, sampleUV);
        const float viewZ = GetDOFViewZ(sampleUV);
        const float2 coc = ComputeDOFCoC(viewZ);
        const float cocMax = max(coc.r, coc.g);
        const float weight = 1.f + cocMax * 2.f + ApplyDOFBokeh(hdr.rgb) * 0.25f;

        colorAccum += hdr.rgb * weight;
        weightSum += weight;
        nearMax = max(nearMax, coc.r);
        farMax = max(farMax, coc.g);
    }

    const float3 prefilteredColor = (weightSum > EPSILON) ? (colorAccum / weightSum) : 0.f;

    Out.CoC = float4(nearMax, farMax, max(nearMax, farMax), 1.f);
    Out.NearBlur = float4(prefilteredColor, nearMax);
    Out.FarBlur = float4(prefilteredColor, farMax);

    return Out;
}

PS_OUT_COLOR PS_DOFBlurNearHalf(PS_IN In)
{
    PS_OUT_COLOR Out;
    Out.Color = SampleDOFBlur(In.Texcoord, true);
    return Out;
}

PS_OUT_COLOR PS_DOFBlurFarHalf(PS_IN In)
{
    PS_OUT_COLOR Out;
    Out.Color = SampleDOFBlur(In.Texcoord, false);
    return Out;
}

PS_OUT_COLOR PS_DOFCompositeHDR(PS_IN In)
{
    PS_OUT_COLOR Out;

    const float4 baseColor = g_DistortionSourceTexture.Sample(LinearClampSampler, In.Texcoord);
    const float2 coc = g_DOFCoCTexture.Sample(LinearClampSampler, In.Texcoord).rg;

    if (g_PP_DOFDebugMode == 1u)
    {
        Out.Color = float4(coc.r, coc.g, max(coc.r, coc.g), 1.f);
        return Out;
    }
    if (g_PP_DOFDebugMode == 2u)
    {
        Out.Color = float4(coc.r.xxx, 1.f);
        return Out;
    }
    if (g_PP_DOFDebugMode == 3u)
    {
        Out.Color = float4(coc.g.xxx, 1.f);
        return Out;
    }

    const float4 nearBlur = g_DOFNearTexture.Sample(LinearClampSampler, In.Texcoord);
    const float4 farBlur = g_DOFFarTexture.Sample(LinearClampSampler, In.Texcoord);

    const float farBlendRaw = saturate(coc.g * g_PP_DOFFarIntensity);
    const float nearBlendRaw = saturate(max(coc.r, nearBlur.a) * g_PP_DOFNearIntensity);
    const float farBlend = ComputeDOFBlendSoftness(farBlendRaw);
    const float nearBlend = ComputeDOFBlendSoftness(nearBlendRaw);

    const float blurWeightSum = farBlend + nearBlend;
    const float3 blurColor = (blurWeightSum > EPSILON)
        ? ((farBlur.rgb * farBlend) + (nearBlur.rgb * nearBlend)) / blurWeightSum
        : baseColor.rgb;

    const float blurAmount = saturate(max(farBlend, nearBlend));
    const float sharpPreserve = (1.f - blurAmount) * (1.f - blurAmount);
    const float3 color = lerp(blurColor, baseColor.rgb, sharpPreserve);

    Out.Color = float4(color, baseColor.a);
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
    PASS_RS_DS_BS_VP(DistortionResolve, RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_DistortionResolve)       // 7
    PASS_RS_DS_BS_VP(LightShaftMaskHalf, RS_Default, DSS_None,  BS_Default, VS_MAIN,    PS_LightShaftMaskHalf)      // 8
    PASS_RS_DS_BS_VP(LightShaftRadialBlurHalf, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_LightShaftRadialBlurHalf) // 9
    PASS_RS_DS_BS_VP(LightShaftCompositeHDR, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_LightShaftCompositeHDR)  // 10
    PASS_RS_DS_BS_VP(DOFPreFilterHalf, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_DOFPreFilterHalf)              // 11
    PASS_RS_DS_BS_VP(DOFBlurNearHalf, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_DOFBlurNearHalf)                // 12
    PASS_RS_DS_BS_VP(DOFBlurFarHalf, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_DOFBlurFarHalf)                  // 13
    PASS_RS_DS_BS_VP(DOFCompositeHDR, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_DOFCompositeHDR)                // 14
}
