#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Utils.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"

Texture2D g_ScreenFxTexture;

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

    // 비네팅 (Vignetting): 화면 외곽을 어둡게 처리하여 중심부 강조
    float2 center = float2(0.5f, 0.5f);
    float dist = distance(In.Texcoord, center);
    float vignette = smoothstep(g_vignetteRadius, g_vignetteRadius - g_vignetteSoftness, dist);
    
    // 대비(Contrast) 살짝 증가시켜 밝기를 더 돋보이게 함
    color.rgb = (color.rgb - 0.5f) * 1.1f + 0.5f;
    
    color.rgb = lerp(g_vignetteColor * color.rgb, color.rgb, vignette);

    Out.Color = color;
    return Out;
}

PS_OUT_BACKBUFFER PS_RadialBlur(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;
    
    float2 dir = g_radialBlurCenter - In.Texcoord;
    float dist = length(dir);
    dir = normalize(dir);
    
    // 외곽일수록 블러 강도가 커지도록 거리 기반 가중치 적용
    float intensity = saturate(dist / max(0.001f, g_radialBlurRadius)) * g_radialBlurIntensity;
    
    float4 color = float4(0, 0, 0, 0);
    const int SAMPLES = 10;
    
    for (int i = 0; i < SAMPLES; ++i)
    {
        float scale = 1.0f - intensity * (float(i) / (float(SAMPLES) - 1.0f));
        float2 sampleUV = g_radialBlurCenter + (In.Texcoord - g_radialBlurCenter) * scale;
        color += g_ScreenFxTexture.Sample(LinearClampSampler, sampleUV);
    }
    color /= float(SAMPLES);
    
    Out.Color = color;
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

    Out.Color = vector(1.f, 1.f, 1.f, 1.f);

    return Out;
}

PS_OUT_BACKBUFFER PS_Distortion(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float2 center = float2(0.5f, 0.5f);
    float2 dirFromCenter = In.Texcoord - center;
    float2 distFromCenter = length(dirFromCenter);

    float2 finalUV = In.Texcoord;

    // Dis Lense 
    float bulgeFactor = 1.f + (distFromCenter * distFromCenter) * g_bulgeIntensity;
    float2 bulgeOffset = dirFromCenter * (bulgeFactor - 1.f);

    // Dis Direction 
    float2 pullOffset = normalize(g_pullDirection) * g_pullIntensity * distFromCenter;

    finalUV += bulgeOffset + pullOffset;

    Out.Color = g_ScreenFxTexture.Sample(LinearClampSampler, finalUV);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(Vignette,         RS_Default, DSS_None, BS_Default, VS_MAIN, PS_Vignette)          
    PASS_RS_DS_BS_VP(RadialBlur,       RS_Default, DSS_None, BS_Default, VS_MAIN, PS_RadialBlur)        
    PASS_RS_DS_BS_VP(ChromaticAberration, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_ChromaticAberration) 
    PASS_RS_DS_BS_VP(Glow,             RS_Default, DSS_None, BS_Default, VS_MAIN, PS_Glow)              
    PASS_RS_DS_BS_VP(Ghosting,              RS_Default, DSS_None, BS_Default, VS_MAIN, PS_Ghosting)
    PASS_RS_DS_BS_VP(Distortion,            RS_Default, DSS_None, BS_Default, VS_MAIN, PS_Distortion)
}

