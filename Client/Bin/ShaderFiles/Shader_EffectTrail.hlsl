#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"

cbuffer cbEffectTrailMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectTrailParams = { 1.f, 1.f, 0.f, 0.f };        // x: intensity, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectTrailAlphaParams = { 0.f, 0.f, 0.f, 0.f };   // x: alphaCutoff, y: alphaErosion, z: useMask, w: reserved
    float4 g_EffectTrailMainUVParams = { 1.f, 1.f, 0.f, 0.f };  // xy: scale, zw: scroll speed
    float4 g_EffectTrailNoiseUVParams = { 1.f, 1.f, 0.f, 0.f }; // xy: scale, zw: scroll speed
    float4 g_EffectTrailMaskUVParams = { 1.f, 1.f, 0.f, 0.f };  // xy: scale, zw: scroll speed
    float4 g_EffectTrailUVPolicyParams = { 1.f, 1.f, 0.f, 0.f }; // x: visible trail length, y: noise uv tiling, zw: reserved
    float4 g_EffectTrailUVModeParams = { 0.f, 0.f, 0.f, 0.f };   // x: main, y: noise, z: mask, w: reserved. 0 Wrap, 1 Stretch, 2 Clamp, 3 Raw
    float g_EffectTrailMaterialTime = 0.f;
    int g_OpacitySource = 0;
    float2 g_EffectTrailPadding;
};

Texture2D g_MainTexture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);

float2 Apply_MaterialUV(float2 uv, float4 uvParams)
{
    return uv * uvParams.xy + uvParams.zw * g_EffectTrailMaterialTime;
}

float2 Build_TrailSlotBaseUV(float2 rawUV, float mode)
{
    const float visibleLength = max(g_EffectTrailUVPolicyParams.x, 0.0001f);
    const float uvTiling = max(g_EffectTrailUVPolicyParams.y, 0.0001f);
    float2 result = float2(rawUV.x, rawUV.y * uvTiling);

    if (mode >= 0.5f && mode < 1.5f)
        result = float2(rawUV.x, saturate(rawUV.y / visibleLength));
    else if (mode >= 2.5f)
        result = rawUV;

    return result;
}

bool Use_ClampSampler(float mode)
{
    return mode >= 0.5f && mode < 2.5f;
}

float4 Sample_MainTexture(float2 uv, float mode)
{
    return Use_ClampSampler(mode)
        ? g_MainTexture.Sample(LinearClampSampler, uv)
        : g_MainTexture.Sample(LinearWrapSampler, uv);
}

float Sample_NoiseTexture(float2 uv, float mode)
{
    return Use_ClampSampler(mode)
        ? g_NoiseTexture.Sample(LinearClampSampler, uv).r
        : g_NoiseTexture.Sample(LinearWrapSampler, uv).r;
}

float Sample_MaskTexture(float2 uv, float mode)
{
    return Use_ClampSampler(mode)
        ? g_MaskTexture.Sample(LinearClampSampler, uv).a
        : g_MaskTexture.Sample(LinearWrapSampler, uv).a;
}

struct VS_IN
{
    float3 position : POSITION;
    row_major float4x4 InstanceMatrix : WORLD;
    float2 lifeTime : TEXCOORD0;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD1;
    float4 segmentParams : TEXCOORD2; // x: 수명 진행률, y: 폭 배율, z: 알파 배율, w: 예비
};

struct VS_OUT
{
    float3 currentBase : POSITION;
    float3 currentTip : TEXCOORD0;
    float3 nextBase : TEXCOORD1;
    float3 nextTip : TEXCOORD2;
    float2 lifeTime : TEXCOORD3;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD4;
    float4 segmentParams : TEXCOORD5;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    Out.currentBase = In.InstanceMatrix[0].xyz;
    Out.currentTip = In.InstanceMatrix[1].xyz;
    Out.nextBase = In.InstanceMatrix[2].xyz;
    Out.nextTip = In.InstanceMatrix[3].xyz;
    Out.lifeTime = In.lifeTime;
    Out.startColor = In.startColor;
    Out.endColor = In.endColor;
    Out.subUVRect = In.subUVRect;
    Out.segmentParams = In.segmentParams;

    return Out;
}

struct GS_IN
{
    float3 currentBase : POSITION;
    float3 currentTip : TEXCOORD0;
    float3 nextBase : TEXCOORD1;
    float3 nextTip : TEXCOORD2;
    float2 lifeTime : TEXCOORD3;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD4;
    float4 segmentParams : TEXCOORD5;
};

struct GS_OUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float2 edgeCoord : TEXCOORD2;
    float4 segmentParams : TEXCOORD3;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
};

void Apply_WidthScale(inout float3 basePosition, inout float3 tipPosition, float widthScale)
{
    const float3 center = (basePosition + tipPosition) * 0.5f;
    const float3 halfWidth = (tipPosition - basePosition) * 0.5f * max(widthScale, 0.0f);
    basePosition = center - halfWidth;
    tipPosition = center + halfWidth;
}

[maxvertexcount(4)]
void GS_MAIN(point GS_IN In[1], inout TriangleStream<GS_OUT> OutStream)
{
    float3 currentBase = In[0].currentBase;
    float3 currentTip = In[0].currentTip;
    float3 nextBase = In[0].nextBase;
    float3 nextTip = In[0].nextTip;
    Apply_WidthScale(currentBase, currentTip, In[0].segmentParams.y);
    Apply_WidthScale(nextBase, nextTip, In[0].segmentParams.y);

    const matrix viewProj = mul(g_ViewMatrix, g_ProjMatrix);

    GS_OUT Out;
    Out.lifeTime = In[0].lifeTime;
    Out.startColor = In[0].startColor;
    Out.endColor = In[0].endColor;
    Out.segmentParams = In[0].segmentParams;

    Out.position = mul(float4(currentBase, 1.f), viewProj);
    Out.texCoord = In[0].subUVRect.xy;
    Out.edgeCoord = float2(0.f, 0.f);
    OutStream.Append(Out);

    Out.position = mul(float4(currentTip, 1.f), viewProj);
    Out.texCoord = float2(In[0].subUVRect.z, In[0].subUVRect.y);
    Out.edgeCoord = float2(0.f, 1.f);
    OutStream.Append(Out);

    Out.position = mul(float4(nextBase, 1.f), viewProj);
    Out.texCoord = float2(In[0].subUVRect.x, In[0].subUVRect.w);
    Out.edgeCoord = float2(1.f, 0.f);
    OutStream.Append(Out);

    Out.position = mul(float4(nextTip, 1.f), viewProj);
    Out.texCoord = In[0].subUVRect.zw;
    Out.edgeCoord = float2(1.f, 1.f);
    OutStream.Append(Out);
}

struct PS_IN
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float2 edgeCoord : TEXCOORD2;
    float4 segmentParams : TEXCOORD3;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

EffectMaterialSurface Resolve_EffectMaterialSurface(PS_IN In)
{
    EffectMaterialSurface surface = (EffectMaterialSurface)0;

    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float mainUVMode = g_EffectTrailUVModeParams.x;
    const float noiseUVMode = g_EffectTrailUVModeParams.y;
    const float maskUVMode = g_EffectTrailUVModeParams.z;
    const float2 mainBaseUV = Build_TrailSlotBaseUV(In.texCoord, mainUVMode);
    const float2 noiseBaseUV = Build_TrailSlotBaseUV(In.texCoord, noiseUVMode);
    const float2 maskBaseUV = Build_TrailSlotBaseUV(In.texCoord, maskUVMode);
    const float2 mainUV = Apply_MaterialUV(mainBaseUV, g_EffectTrailMainUVParams);
    const float2 noiseUV = Apply_MaterialUV(noiseBaseUV, g_EffectTrailNoiseUVParams);
    const float2 maskUV = Apply_MaterialUV(maskBaseUV, g_EffectTrailMaskUVParams);
    const float4 tex = Sample_MainTexture(mainUV, mainUVMode);
    const float sideFade = max(In.segmentParams.w, 0.0001f);
    const float sideAlpha =
        smoothstep(0.0f, sideFade, In.edgeCoord.y) *
        smoothstep(0.0f, sideFade, 1.0f - In.edgeCoord.y);
    const float4 segmentColor = lerp(In.startColor, In.endColor, lifeProgress);
    const float opacityPower = max(g_EffectTrailParams.y, 0.0001f);
    const float alphaCutoff = saturate(g_EffectTrailAlphaParams.x);
    const float alphaErosion = saturate(g_EffectTrailAlphaParams.y);
    const float selectedOpacity = Effect_SelectOpacity(tex, g_OpacitySource);
    float noiseRaw = 1.f;
    float maskAlpha = 1.f;

    const float noiseStrength = saturate(g_EffectTrailParams.z);
    if (0 != g_EffectTrailParams.w && g_EffectTrailParams.z > 0.f)
        noiseRaw = Sample_NoiseTexture(noiseUV, noiseUVMode);

    if (0 != g_EffectTrailAlphaParams.z)
        maskAlpha = Sample_MaskTexture(maskUV, maskUVMode);

    const float edgeDistance = abs(In.edgeCoord.y - 0.5f) * 2.f;
    const float edgeWeight = smoothstep(0.35f, 1.f, edgeDistance);
    const float breakupNoise = lerp(1.f, noiseRaw, noiseStrength);
    const float edgeBreakup = lerp(1.f, noiseRaw, noiseStrength * edgeWeight);

    float coverage = maskAlpha;

    coverage = Effect_ApplyAlphaErosion(coverage, maskAlpha * edgeBreakup, alphaErosion);

    const float energyVariation = lerp(1.f, lerp(0.75f, 1.25f, noiseRaw), noiseStrength);

    surface.color = segmentColor.rgb * g_Tint.rgb * tex.rgb * energyVariation;
    surface.coverage = Effect_BuildCoverage(
        selectedOpacity,
        opacityPower,
        coverage,
        breakupNoise,
        segmentColor.a * g_Tint.a * saturate(In.segmentParams.z) * sideAlpha
    );

    if (surface.coverage < max(alphaCutoff, 0.01f))
        discard;

    return surface;
}

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMaterialSurface(In);
    Out.color = Effect_ComposeAlphaBlend(surface, g_EffectTrailParams.x);
    return Out;
}

PS_OUT PS_ADDITIVE(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMaterialSurface(In);
    Out.color = Effect_ComposeAdditive(surface, g_EffectTrailParams.x);
    return Out;
}

PS_OUT PS_MODULATE(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMaterialSurface(In);
    Out.color = Effect_ComposeModulate(surface, g_EffectTrailParams.x);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(AlphaBlendPass, RS_CullNone, DSS_DepthRead, BS_AlphaBlend, VS_MAIN, GS_MAIN, PS_MAIN) // 0
    PASS_RS_DS_BS_VGP(AdditivePass, RS_CullNone, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, GS_MAIN, PS_ADDITIVE) // 1
    PASS_RS_DS_BS_VGP(ModulatePass, RS_CullNone, DSS_DepthRead, BS_EffectModulate, VS_MAIN, GS_MAIN, PS_MODULATE) // 2
}
