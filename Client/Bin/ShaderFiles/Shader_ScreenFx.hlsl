#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Utils.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"

Texture2D g_ScreenFxTexture;
Texture2D g_NoiseTexture;
Texture2D g_ScreenMaskTexture;
Texture2D g_SmudgeTexture;

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

    float4x4 matWV = mul(g_WorldMatrix, g_ScreenViewMatrix);
    float4x4 matWVP = mul(matWV, g_ScreenProjMatrix);

    Out.Position = mul(float4(In.Position, 1.f), matWVP);
    Out.Texcoord = In.Texcoord;

    return Out;
}

struct PS_IN
{
    float4 Position : SV_POSITION;
    float2 Texcoord : TEXCOORD0;
};

struct PS_OUT_BACKBUFFER
{
    vector Color : SV_TARGET0;
};

PS_OUT_BACKBUFFER PS_Vignette(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;
    float4 color = g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord);

    float2 center = float2(0.5f, 0.5f);
    float dist = distance(In.Texcoord, center);
    float vignette = smoothstep(g_vignetteRadius, g_vignetteRadius - g_vignetteSoftness, dist);

    // 대비(Contrast) 살짝 증가시켜 밝기를 더 돋보이게 함
    float smudgeMask = g_SmudgeTexture.Sample(LinearClampSampler, In.Texcoord).r;
    vignette *= smudgeMask;

    color.rgb = (color.rgb - 0.5f) * 1.1f + 0.5f;
    color.rgb = lerp(g_vignetteColor * color.rgb, color.rgb, vignette);
    Out.Color = color;

    return Out;
}

PS_OUT_BACKBUFFER PS_RadialBlur(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float2 dir = In.Texcoord - g_radialBlurCenter;
    float dist = length(dir);

    float weight = smoothstep(0.f, g_radialBlurRadius, dist);
    float blurAmount = clamp(g_radialBlurIntensity * weight, 0.f, 0.5f);

    Out.Color = float4(0, 0, 0, 0);
    int samples = 5;

    for (int i = 0; i < samples; i++)
    {
        float scale = 1.0f - blurAmount * (i / (float) (samples - 1));
        float2 sampleUV = g_radialBlurCenter + dir * scale;
        Out.Color += g_ScreenFxTexture.Sample(LinearSampler, sampleUV);
    }
    
    Out.Color /= samples;
    return Out;
}

PS_OUT_BACKBUFFER PS_ChromaticAberration(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float2 center = float2(0.5f, 0.5f);
    float2 dir = In.Texcoord - center;
    float dist = length(dir);

    // 중심부보다 외곽에서 색수차가 더 강하게 일어나도록 설정
    float offsetAmount = dist * g_chromaticAberrationIntensity;

    float2 redUV = In.Texcoord - dir * offsetAmount;
    float2 greenUV = In.Texcoord;
    float2 blueUV = In.Texcoord + dir * offsetAmount;

    float r = g_ScreenFxTexture.Sample(LinearClampSampler, redUV).r;
    float g = g_ScreenFxTexture.Sample(LinearClampSampler, greenUV).g;

    float b = g_ScreenFxTexture.Sample(LinearClampSampler, blueUV).b;
    float a = g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord).a;

    Out.Color = float4(r, g, b, a);
    return Out;
}

PS_OUT_BACKBUFFER PS_Glow(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;
    float4 baseColor = g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord);

    // 간단한 박스 블러 형태의 글로우 (임시 가짜 블룸)
    float4 blurColor = float4(0,0,0,0);
    float2 texOffset = float2(1.0f / g_ScreenSize.x, 1.0f / g_ScreenSize.y) * 2.0f;

    blurColor += g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord + float2( texOffset.x,  texOffset.y));
    blurColor += g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord + float2(-texOffset.x,  texOffset.y));
    blurColor += g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord + float2( texOffset.x, -texOffset.y));
    blurColor += g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord + float2(-texOffset.x, -texOffset.y));
    blurColor *= 0.25f;

    // 밝은 부분만 글로우가 되도록 Threshold 적용
    float luminance = dot(blurColor.rgb, float3(0.299f, 0.587f, 0.114f));
    float glowMask = saturate((luminance - 0.5f) * 2.0f);

    Out.Color = baseColor + (blurColor * glowMask * 0.5f); // 강도는 외부 CBuffer 연동가능
    return Out;
}

PS_OUT_BACKBUFFER PS_Ghosting(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    Out.Color = g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord);

    return Out;
}

PS_OUT_BACKBUFFER PS_Distortion(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float2 center = float2(0.5f, 0.5f);
    float2 dirFromCenter = In.Texcoord - center;
    float2 distFromCenter = length(dirFromCenter);

    float2 finalUV = In.Texcoord;

    // Dis Lense (Bulge)
    // 양수 bulgeIntensity일 때 볼록 렌즈처럼 확대되도록 (UV를 중심쪽으로 당김)
    float bulgeFactor = 1.f - (distFromCenter * distFromCenter) * g_bulgeIntensity;
    float2 bulgeOffset = dirFromCenter * (bulgeFactor - 1.f);

    // Dis Direction (Pull)
    float2 pullOffset = float2(0.f, 0.f);
    float pullLen = length(g_pullDirection);
    float2 pullDir = (pullLen > 0.0001f) ? (g_pullDirection / pullLen) : float2(0.f, 0.f);

    if (pullLen > 0.0001f)
    {
        pullOffset = pullDir * g_pullIntensity * distFromCenter;
    }

    finalUV += bulgeOffset + pullOffset;

    // Chromatic Aberration (무지개 효과) pullDirection이 있으면 해당 방향으로, 없으면 방사형(Radial)으로 색수차 발생
    float caStrength = (abs(g_bulgeIntensity) + abs(g_pullIntensity)) * 0.03f * distFromCenter;
    float2 caOffset = (pullLen > 0.0001f) ? (pullDir * caStrength) : (dirFromCenter * caStrength);

    // RGB 분리 샘플링
    float r = g_ScreenFxTexture.Sample(LinearClampSampler, finalUV - caOffset).r;
    float g = g_ScreenFxTexture.Sample(LinearClampSampler, finalUV).g;
    float b = g_ScreenFxTexture.Sample(LinearClampSampler, finalUV + caOffset).b;
    float a = g_ScreenFxTexture.Sample(LinearClampSampler, finalUV).a;

    Out.Color = float4(r, g, b, a);
    return Out;
}

PS_OUT_BACKBUFFER PS_CircularWipeTransition(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float4 currentSceneColor = g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord);
    float4 capturedSceneColor = g_ScreenMaskTexture.Sample(LinearClampSampler, In.Texcoord);
    if (g_transitionUseBlackSource > 0.5f)
        capturedSceneColor = float4(0.f, 0.f, 0.f, 1.f);

    float blackFade = saturate((g_wipeRadius - 0.4f) * 2.f);
    capturedSceneColor.rgb = lerp(capturedSceneColor.rgb, float3(0.f, 0.f, 0.f), blackFade);

    float2 center = float2(0.5, 0.5);
    float distForVignette = distance(In.Texcoord, center);
    float vignette = smoothstep(g_vignetteRadius, g_vignetteRadius - g_vignetteSoftness, distForVignette);

    float smudgeMask = g_SmudgeTexture.Sample(LinearSampler, In.Texcoord).r;
    vignette *= smudgeMask;
    
    // 기존 blackFade(0.4부터) 보다 조금 더 일찍(0.2부터) 등장하고 빠르게 진해지도록 조절
    float smudgeFade = saturate((g_wipeRadius - 0.2f) * 1.5f);
    float currentVignette = lerp(1.0f, vignette, smudgeFade);
    capturedSceneColor.rgb = lerp(g_vignetteColor * capturedSceneColor.rgb, capturedSceneColor.rgb, currentVignette);
    
    float aspect = g_ScreenSize.x / max(1.0, g_ScreenSize.y);
    float2 diff = In.Texcoord - center;
    diff.x *= aspect;

    float dist = length(diff);
    float edgeSoftness = max(0.001f, g_edgeSoftness);
    float mask = smoothstep(g_wipeRadius, g_wipeRadius + edgeSoftness, dist);

    Out.Color = lerp(currentSceneColor, capturedSceneColor, mask);

    return Out;

}

float Compute_ScreenFxShapeMask(float2 texcoord)
{
    float shapeMode = g_screenFxParams.y;
    float invertShape = g_screenFxParams.z;
    float mask = 1.f;

    if (shapeMode >= 0.5f)
    {
        float aspect = g_ScreenSize.x / max(1.0f, g_ScreenSize.y);
        float2 diff = texcoord - g_screenFxShape.xy;
        diff.x *= aspect;

        float dist = length(diff);
        float radius = max(0.f, g_screenFxShape.z);
        float softness = max(0.001f, g_screenFxShape.w);
        mask = 1.f - smoothstep(radius, radius + softness, dist);
    }

    if (invertShape >= 0.5f)
        mask = 1.f - mask;

    return saturate(mask);
}

PS_OUT_BACKBUFFER PS_Flash(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float4 sceneColor = g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord);
    float shapeMask = Compute_ScreenFxShapeMask(In.Texcoord);
    float amount = saturate(g_screenFxParams.x * shapeMask);
    float3 flashColor = g_flashColor.rgb * max(0.f, g_screenFxParams.w);

    Out.Color = sceneColor;
    Out.Color.rgb = lerp(sceneColor.rgb, flashColor, amount);

    return Out;
}

PS_OUT_BACKBUFFER PS_WorldDesaturate(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float4 sceneColor = g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord);
    float shapeMask = Compute_ScreenFxShapeMask(In.Texcoord);
    float amount = saturate(g_screenFxParams.x * shapeMask);
    float grayscale = dot(sceneColor.rgb, float3(0.299f, 0.587f, 0.114f));

    Out.Color = sceneColor;
    Out.Color.rgb = lerp(sceneColor.rgb, grayscale.xxx, amount);

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(Vignette,                  RS_Default, DSS_None, BS_Default, VS_MAIN, PS_Vignette)
    PASS_RS_DS_BS_VP(RadialBlur,                RS_Default, DSS_None, BS_Default, VS_MAIN, PS_RadialBlur)
    PASS_RS_DS_BS_VP(ChromaticAberration,       RS_Default, DSS_None, BS_Default, VS_MAIN, PS_ChromaticAberration)
    PASS_RS_DS_BS_VP(Glow,                      RS_Default, DSS_None, BS_Default, VS_MAIN, PS_Glow)
    PASS_RS_DS_BS_VP(Ghosting,                  RS_Default, DSS_None, BS_Default, VS_MAIN, PS_Ghosting)
    PASS_RS_DS_BS_VP(Distortion,                RS_Default, DSS_None, BS_Default, VS_MAIN, PS_Distortion)
    PASS_RS_DS_BS_VP(PS_CircularWipeTransition, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_CircularWipeTransition)
    PASS_RS_DS_BS_VP(Flash,                     RS_Default, DSS_None, BS_Default, VS_MAIN, PS_Flash)
    PASS_RS_DS_BS_VP(FlashExclude,              RS_Default, DSS_ScreenFxExclude, BS_Default, VS_MAIN, PS_Flash)
    PASS_RS_DS_BS_VP(WorldDesaturate,           RS_Default, DSS_None, BS_Default, VS_MAIN, PS_WorldDesaturate)
    PASS_RS_DS_BS_VP(WorldDesaturateExclude,    RS_Default, DSS_ScreenFxExclude, BS_Default, VS_MAIN, PS_WorldDesaturate)
}
