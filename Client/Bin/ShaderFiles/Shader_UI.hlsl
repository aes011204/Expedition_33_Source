#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

Texture2D g_Texture;
Texture2D g_MaskTexture;

bool g_IsOverlay;
bool g_UseGrayScale;
float4 g_GrayAmount;
float4 g_Color = float4(1.f, 1.f, 1.f, 1.f);


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

    Out.vColor = g_Texture.Sample(LinearClampSampler, In.vTexcoord) * g_Color;

    float4 finalColor = Out.vColor;

    if(g_UseGrayScale)
    {
        float gray = dot(Out.vColor.rgb, float3(0.299f, 0.587f, 0.114f));
        float3 grayColor = float3(gray, gray, gray);

    //    finalColor.rgb = lerp(Out.vColor.rgb, grayColor, g_GrayAmount);
        finalColor.rgb = grayColor * 0.5f;
    }

    if(g_IsOverlay)
    {
        float3 invertedRgb = 1.f - Out.vColor.rgb;

        finalColor.rgb = lerp(Out.vColor.rgb, invertedRgb, Out.vColor.a);
    }


    Out.vColor = finalColor;

    return Out;
}

PS_OUT PS_MASK(PS_IN In)
{
    PS_OUT Out;

    Out.vColor = g_Texture.Sample(LinearClampSampler, In.vTexcoord) * g_Color;
    Out.vColor.a *= g_MaskTexture.Sample(LinearClampSampler, In.vTexcoord).a;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(NormalPass,    RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_NORMAL) // 0
    PASS_RS_DS_BS_VP(MaskPass,      RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_MASK)   // 1
}
