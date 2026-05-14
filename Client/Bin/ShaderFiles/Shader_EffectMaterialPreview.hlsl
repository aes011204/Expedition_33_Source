#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"

cbuffer cbEffectMaterialPreview : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMaterialPreviewParams = { 1.f, 1.f, 0.f, 0.f }; // x: intensity, y: opacityPower, z: noiseStrength, w: useNoise
    int g_OpacitySource = 0;
    float3 g_EffectMaterialPreviewPadding;
};

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);

struct VS_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    const float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    const float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.position = mul(float4(In.position, 1.f), matWVP);
    Out.normal = normalize(mul(float4(In.normal, 0.f), g_WorldMatrix).xyz);
    Out.texCoord = In.texCoord;

    return Out;
}

struct PS_IN
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float4 symbol = g_Texture.Sample(LinearClampSampler, In.texCoord);
    const float opacityPower = max(g_EffectMaterialPreviewParams.y, 0.0001f);
    float selectedOpacity = symbol.a;
    float noiseFactor = 1.f;

    if (g_OpacitySource == 1)
        selectedOpacity = symbol.r;
    else if (2 == g_OpacitySource)
        selectedOpacity = dot(symbol.rgb, float3(0.299f, 0.587f, 0.114f));

    if (g_EffectMaterialPreviewParams.w != 0 && g_EffectMaterialPreviewParams.z > 0.f)
    {
        const float noise = g_NoiseTexture.Sample(LinearClampSampler, In.texCoord).r;
        noiseFactor = lerp(1.f, noise, saturate(g_EffectMaterialPreviewParams.z));
    }

    const float opacity = pow(saturate(selectedOpacity), opacityPower);
    const float facing = saturate(abs(In.normal.z) * 0.35f + 0.65f);
    Out.color.rgb = symbol.rgb * g_Tint.rgb * g_EffectMaterialPreviewParams.x * noiseFactor * facing;
    Out.color.a = opacity * g_Tint.a;

    if (Out.color.a < 0.01f)
        discard;

    return Out;
}

technique11 DefaultTechnique
{
    // Plane preview
    PASS_RS_DS_BS_VP(AlphaBlendPass, RS_CullNone, DSS_Default, BS_AlphaBlend, VS_MAIN, PS_MAIN) // 0
    PASS_RS_DS_BS_VP(AdditivePass, RS_CullNone, DSS_Default, BS_Blend, VS_MAIN, PS_MAIN)        // 1

    // Sphere preview
    PASS_RS_DS_BS_VP(SphereAlphaBlendPass, RS_Default, DSS_Default, BS_AlphaBlend, VS_MAIN, PS_MAIN) // 2
    PASS_RS_DS_BS_VP(SphereAdditivePass, RS_Default, DSS_Default, BS_Blend, VS_MAIN, PS_MAIN)        // 3
}
