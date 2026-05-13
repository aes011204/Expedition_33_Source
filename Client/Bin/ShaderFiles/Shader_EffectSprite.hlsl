#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"

cbuffer cbCamera : register(b1)
{
    float4x4 g_ViewMatrix;
    float4x4 g_ProjMatrix;
    float4 g_vBillboardTarget;
};

cbuffer cbEffectSpriteMaterial : register(b2)
{
    float4 g_vTint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_vEffectSpriteParams = { 1.f, 1.f, 0.f, 0.f }; // x: intensity, y: opacityPower, z: noiseStrength, w: useNoise
    int g_iOpacitySource = 0;
    float3 g_vEffectSpritePadding;
};

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);

struct VS_IN
{
    float3 vPosition : POSITION;
    row_major float4x4 InstanceMatrix : WORLD;
    float2 vLifeTime : TEXCOORD0;
    float4 vStartColor : COLOR0;
    float4 vEndColor : COLOR1;
};

struct VS_OUT
{
    float4 vWorldPosition : POSITION;
    float2 vSize : TEXCOORD0;
    float2 vLifeTime : TEXCOORD1;
    float4 vStartColor : COLOR0;
    float4 vEndColor : COLOR1;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    const float4 vWorldPosition = mul(float4(In.vPosition, 1.f), In.InstanceMatrix);
    Out.vWorldPosition = vWorldPosition;
    Out.vSize = float2(length(In.InstanceMatrix[0].xyz), length(In.InstanceMatrix[1].xyz));
    Out.vLifeTime = In.vLifeTime;
    Out.vStartColor = In.vStartColor;
    Out.vEndColor = In.vEndColor;

    return Out;
}

struct GS_IN
{
    float4 vWorldPosition : POSITION;
    float2 vSize : TEXCOORD0;
    float2 vLifeTime : TEXCOORD1;
    float4 vStartColor : COLOR0;
    float4 vEndColor : COLOR1;
};

struct GS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexCoord : TEXCOORD0;
    float2 vLifeTime : TEXCOORD1;
    float4 vStartColor : COLOR0;
    float4 vEndColor : COLOR1;
};

[maxvertexcount(4)]
void GS_MAIN(point GS_IN In[1], inout TriangleStream<GS_OUT> OutStream)
{
    const float3 vParticlePosition = In[0].vWorldPosition.xyz;
    float3 vLook = g_vBillboardTarget.xyz - vParticlePosition;

    if (length(vLook) < 0.0001f)
        vLook = float3(0.f, 0.f, 1.f);
    else
        vLook = normalize(vLook);

    float3 vWorldUp = float3(0.f, 1.f, 0.f);
    float3 vRight = cross(vWorldUp, vLook);

    if (length(vRight) < 0.0001f)
        vRight = cross(float3(0.f, 0.f, 1.f), vLook);

    vRight = normalize(vRight);
    float3 vUp = normalize(cross(vLook, vRight));
    vRight *= In[0].vSize.x * 0.5f;
    vUp *= In[0].vSize.y * 0.5f;

    const matrix ViewProjMatrix = mul(g_ViewMatrix, g_ProjMatrix);
    GS_OUT Out[4];

    Out[0].vPosition = mul(float4(vParticlePosition - vRight + vUp, 1.f), ViewProjMatrix);
    Out[0].vTexCoord = float2(0.f, 0.f);
    Out[0].vLifeTime = In[0].vLifeTime;
    Out[0].vStartColor = In[0].vStartColor;
    Out[0].vEndColor = In[0].vEndColor;

    Out[1].vPosition = mul(float4(vParticlePosition + vRight + vUp, 1.f), ViewProjMatrix);
    Out[1].vTexCoord = float2(1.f, 0.f);
    Out[1].vLifeTime = In[0].vLifeTime;
    Out[1].vStartColor = In[0].vStartColor;
    Out[1].vEndColor = In[0].vEndColor;

    Out[2].vPosition = mul(float4(vParticlePosition - vRight - vUp, 1.f), ViewProjMatrix);
    Out[2].vTexCoord = float2(0.f, 1.f);
    Out[2].vLifeTime = In[0].vLifeTime;
    Out[2].vStartColor = In[0].vStartColor;
    Out[2].vEndColor = In[0].vEndColor;

    Out[3].vPosition = mul(float4(vParticlePosition + vRight - vUp, 1.f), ViewProjMatrix);
    Out[3].vTexCoord = float2(1.f, 1.f);
    Out[3].vLifeTime = In[0].vLifeTime;
    Out[3].vStartColor = In[0].vStartColor;
    Out[3].vEndColor = In[0].vEndColor;

    OutStream.Append(Out[0]);
    OutStream.Append(Out[1]);
    OutStream.Append(Out[2]);
    OutStream.Append(Out[3]);
}

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float2 vTexCoord : TEXCOORD0;
    float2 vLifeTime : TEXCOORD1;
    float4 vStartColor : COLOR0;
    float4 vEndColor : COLOR1;
};

struct PS_OUT
{
    float4 vColor : SV_TARGET0;
};

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float fLifeMax = max(In.vLifeTime.x, 0.0001f);
    const float fLifeProgress = saturate(In.vLifeTime.y / fLifeMax);
    const float fLifeAlphaFade = saturate(1.f - fLifeProgress);
    const float4 vParticleColor = lerp(In.vStartColor, In.vEndColor, fLifeProgress);
    const float4 vSymbol = g_Texture.Sample(LinearClampSampler, In.vTexCoord);
    const float fOpacityPower = max(g_vEffectSpriteParams.y, 0.0001f);
    float fSelectedOpacity = vSymbol.a;
    float fNoiseFactor = 1.f;

    if (1 == g_iOpacitySource)
        fSelectedOpacity = vSymbol.r;
    else if (2 == g_iOpacitySource)
        fSelectedOpacity = dot(vSymbol.rgb, float3(0.299f, 0.587f, 0.114f));

    if (0 != g_vEffectSpriteParams.w && g_vEffectSpriteParams.z > 0.f)
    {
        const float fNoise = g_NoiseTexture.Sample(LinearClampSampler, In.vTexCoord).r;
        fNoiseFactor = lerp(1.f, fNoise, saturate(g_vEffectSpriteParams.z));
    }

    const float fOpacity = pow(saturate(fSelectedOpacity), fOpacityPower);
    Out.vColor.rgb = vSymbol.rgb * vParticleColor.rgb * g_vTint.rgb * g_vEffectSpriteParams.x * fNoiseFactor;
    Out.vColor.a = fOpacity * vParticleColor.a * g_vTint.a * fLifeAlphaFade;

    if (Out.vColor.a < 0.01f)
        discard;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(DefaultPass, RS_CullNone, DSS_Default, BS_AlphaBlend, VS_MAIN, GS_MAIN, PS_MAIN) // 0
}
