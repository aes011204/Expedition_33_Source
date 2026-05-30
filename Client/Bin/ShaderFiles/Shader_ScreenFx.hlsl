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

    if (In.Texcoord.x <= (1.f / 3.f))
        color.rgb = float3(1.f, 0.f, 0.f);

    Out.Color = float4(color.rgb, 1.f);
    return Out;
}

PS_OUT_BACKBUFFER PS_Ghosting(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;
    float4 color = g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord);

    if (In.Texcoord.x > (1.f / 3.f) && In.Texcoord.x <= (2.f / 3.f))
        color.rgb = float3(0.f, 1.f, 0.f);

    Out.Color = float4(color.rgb, 1.f);
    return Out;
}

PS_OUT_BACKBUFFER PS_BattleTransition(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;
    float4 color = g_ScreenFxTexture.Sample(LinearClampSampler, In.Texcoord);

    if (In.Texcoord.x > (2.f / 3.f))
        color.rgb = float3(0.f, 0.f, 1.f);

    Out.Color = float4(color.rgb, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(Vignette,         RS_Default, DSS_None, BS_Default, VS_MAIN, PS_Vignette)          // 0
    PASS_RS_DS_BS_VP(Ghosting,         RS_Default, DSS_None, BS_Default, VS_MAIN, PS_Ghosting)          // 1
    PASS_RS_DS_BS_VP(BattleTransition, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_BattleTransition)  // 2
}
