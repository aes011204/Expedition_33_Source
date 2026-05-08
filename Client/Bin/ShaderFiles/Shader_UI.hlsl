#include "Engine_Shader_Defines.hlsli"

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

texture2D g_Texture;
texture2D g_MaskTexture;

float4 g_Color = float4(1.f, 1.f, 1.f, 1.f);

sampler DefaultSampler = sampler_state
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

RasterizerState RS_UI
{
    FillMode = Solid;
    CullMode = none;
    FrontCounterClockwise = false;
};

struct VS_IN
{
    float3 vPosition : POSITION;
    float2 vTexcoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP);
    Out.vTexcoord = In.vTexcoord;

    return Out;
}

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
};

struct PS_OUT
{
    float4 vColor : SV_TARGET0;
};

PS_OUT PS_NORMAL(PS_IN In)
{
    PS_OUT Out;

    Out.vColor = g_Texture.Sample(DefaultSampler, In.vTexcoord) * g_Color;

    return Out;
}

PS_OUT PS_MASK(PS_IN In)
{
    PS_OUT Out;

    Out.vColor = g_Texture.Sample(DefaultSampler, In.vTexcoord) * g_Color;
    Out.vColor.a *= g_MaskTexture.Sample(DefaultSampler, In.vTexcoord).a;

    return Out;
}

technique11 DefaultTechnique
{
    pass NormalPass
    {
        SetRasterizerState(RS_UI);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_NORMAL();
    }

    pass MaskPass
    {
        SetRasterizerState(RS_UI);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_MASK();
    }
}
