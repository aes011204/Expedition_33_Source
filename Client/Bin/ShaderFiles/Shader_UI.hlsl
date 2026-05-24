#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

Texture2D g_Texture;
Texture2D g_MaskTexture;
Texture2D g_PatternMaskTexture;

bool g_IsOverlay;
bool g_UseGrayScale;
float4 g_GrayAmount;
float4 g_Color = float4(1.f, 1.f, 1.f, 1.f);

// 원형 마스크ㄴ
float2 g_HoleCenterUV = float2(0.5f, 0.5f);
float g_HoleRadius = 0.0f;
float g_HoleFeather = 0.05f;
float g_ViewportAspect = 1.0f;
bool g_UseFillRatio = false;
float g_FillRatio = 1.0f;
uint g_FillOrigin = 0;

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

PS_OUT PS_BLACK_WHITE_MASK(PS_IN In)
{
    PS_OUT Out;

    const float3 lumaWeights = float3(0.299f, 0.587f, 0.114f);
    const float mask = saturate(dot(g_MaskTexture.Sample(LinearClampSampler, In.vTexcoord).rgb, lumaWeights));

    Out.vColor = g_Texture.Sample(LinearClampSampler, In.vTexcoord) * g_Color;
    Out.vColor.a *= mask;

    return Out;
}

PS_OUT PS_CIRCULAR_HOLE(PS_IN In)
{
    PS_OUT Out;

    float4 baseColor = g_Texture.Sample(LinearClampSampler, In.vTexcoord) * g_Color;
    float2 delta = In.vTexcoord - g_HoleCenterUV;
    delta.x *= g_ViewportAspect;
    float dist = length(delta);

    // 반경 안쪽은 투명, 바깥은 불투명
    float alpha = smoothstep(g_HoleRadius - g_HoleFeather, g_HoleRadius, dist);

    Out.vColor = baseColor;
    Out.vColor.a *= alpha;

    return Out;
}

PS_OUT PS_PATTERN_OVERLAY(PS_IN In)
{
    PS_OUT Out;

    const float3 lumaWeights = float3(0.299f, 0.587f, 0.114f);
    const float4 pattern = g_Texture.Sample(LinearClampSampler, In.vTexcoord);
    const float paintMask = saturate(dot(g_MaskTexture.Sample(LinearClampSampler, In.vTexcoord).rgb, lumaWeights));
    const float patternMask = saturate(dot(g_PatternMaskTexture.Sample(LinearClampSampler, In.vTexcoord).rgb, lumaWeights));
    const float patternCoverage = saturate(min(pattern.a, dot(pattern.rgb, lumaWeights)));

    Out.vColor.rgb = g_Color.rgb;
    Out.vColor.a = patternCoverage * paintMask * patternMask * g_Color.a;

    return Out;
}

PS_OUT PS_FILL_RATIO(PS_IN In)
{
    PS_OUT Out;

    float2 uv = In.vTexcoord;
    float visible = 1.0f;

    if (g_FillOrigin == 0)
        visible = step(uv.x, g_FillRatio);
    else if (g_FillOrigin == 1)
        visible = step(1.0f - g_FillRatio, uv.x);
    else if (g_FillOrigin == 2)
        visible = step(uv.y, g_FillRatio);
    else
        visible = step(1.0f - g_FillRatio, uv.y);

    Out.vColor = g_Texture.Sample(LinearClampSampler, uv) * g_Color;
    Out.vColor.a *= visible;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(NormalPass,    RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_NORMAL) // 0
    PASS_RS_DS_BS_VP(MaskPass,      RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_MASK)   // 1
    PASS_RS_DS_BS_VP(CircularHolePass, RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_CIRCULAR_HOLE) // 2
    PASS_RS_DS_BS_VP(PatternOverlayPass, RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_PATTERN_OVERLAY) // 3
    PASS_RS_DS_BS_VP(FillRatioPass, RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_FILL_RATIO) // 4
    PASS_RS_DS_BS_VP(BlackWhiteMaskPass, RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_BLACK_WHITE_MASK) // 5
}
