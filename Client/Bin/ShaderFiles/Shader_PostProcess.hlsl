#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Utils.hlsl"
#include "Engine_Shader_PostProcess.hlsl"

//==================================================
// TEXTURE
//==================================================
Texture2D g_HDRTexture;

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

PS_OUT_BACKBUFFER PS_ToneMapping(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    float4 hdr = g_HDRTexture.Sample(LinearClampSampler, In.Texcoord);

    float3 color = hdr.rgb;
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

    // 최종 출력 화면은 LDR
    Out.Color = float4(saturate(color), hdr.a);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(HDRDebug,      RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_ToneMapping)
    // PASS_RS_DS_BS_VP(BloomExtract, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_MAIN_BLOOM_EXTRACT)
    // PASS_RS_DS_BS_VP(BloomBlurH, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_MAIN_BLOOM_BLUR_H)
    // PASS_RS_DS_BS_VP(BloomBlurV, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_MAIN_BLOOM_BLUR_V)
    PASS_RS_DS_BS_VP(ToneMapping,   RS_Default, DSS_None,   BS_Default, VS_MAIN,    PS_ToneMapping)
}
