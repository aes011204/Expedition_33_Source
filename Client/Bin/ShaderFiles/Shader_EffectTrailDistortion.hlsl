#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Shader_EffectDistortionCommon.hlsli"

cbuffer cbEffectTrailDistortion : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectDistortionParams = { 0.f, 1.f, 0.f, 0.f }; // x: intensity, y: presence, z: useFlow, w: material time
    float4 g_DistortionScreenSize = { 1.f, 1.f, 0.f, 0.f };
    float2 g_FlowUVPolicyParams = { 0.f, 0.f };
    float2 g_FlowUVScale = { 1.f, 1.f };
    float2 g_FlowUVOffset = { 0.f, 0.f };
    float2 g_FlowUVScrollSpeed = { 0.f, 0.f };
    float g_FlowUVTilingMode = 0.f;
    float g_FlowUVRotation = 0.f;
    float g_TrailDistortionPadding = 0.f;
#include "Shader_EffectMaterialModulationPayloadGlobals.hlsli"
};

#include "Shader_EffectMaterialScalarModulation.hlsli"

Texture2D g_FlowTexture : register(t3);

float2 Build_FlowUV(float2 rawUV)
{
    return EffectDistortion_BuildAxisSeparatedUV(
        rawUV,
        g_FlowUVScale,
        g_FlowUVOffset,
        g_FlowUVScrollSpeed,
        g_EffectDistortionParams.w,
        g_FlowUVPolicyParams,
        g_FlowUVRotation
    );
}

float2 Sample_FlowTexture(float2 uv)
{
    return g_FlowTexture.Sample(
        LinearWrapSampler,
        EffectDistortion_ApplyAxisSeparatedAddress(uv, g_FlowUVPolicyParams)
    ).rg;
}

struct VS_IN
{
    float3 position : POSITION;
    row_major float4x4 InstanceMatrix : WORLD;
    float2 lifeTime : TEXCOORD0;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD1;
    float4 segmentParams : TEXCOORD2; // x: 수명 진행률, y: reserved, z: 알파 배율, w: side fade
    float4 coreColorRgb : TEXCOORD6;
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

[maxvertexcount(4)]
void GS_MAIN(point GS_IN In[1], inout TriangleStream<GS_OUT> OutStream)
{
    const matrix viewProj = mul(g_ViewMatrix, g_ProjMatrix);

    GS_OUT Out;
    Out.lifeTime = In[0].lifeTime;
    Out.startColor = In[0].startColor;
    Out.endColor = In[0].endColor;
    Out.segmentParams = In[0].segmentParams;

    Out.position = mul(float4(In[0].currentBase, 1.f), viewProj);
    Out.texCoord = In[0].subUVRect.xy;
    Out.edgeCoord = float2(0.f, 0.f);
    OutStream.Append(Out);

    Out.position = mul(float4(In[0].currentTip, 1.f), viewProj);
    Out.texCoord = float2(In[0].subUVRect.z, In[0].subUVRect.y);
    Out.edgeCoord = float2(0.f, 1.f);
    OutStream.Append(Out);

    Out.position = mul(float4(In[0].nextBase, 1.f), viewProj);
    Out.texCoord = float2(In[0].subUVRect.x, In[0].subUVRect.w);
    Out.edgeCoord = float2(1.f, 0.f);
    OutStream.Append(Out);

    Out.position = mul(float4(In[0].nextTip, 1.f), viewProj);
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

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float sideFade = max(In.segmentParams.w, 0.0001f);
    const float sideAlpha =
        smoothstep(0.0f, sideFade, In.edgeCoord.y) *
        smoothstep(0.0f, sideFade, 1.0f - In.edgeCoord.y);
    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float segmentAlpha = lerp(In.startColor.a, In.endColor.a, lifeProgress);
    const float trailCoverage = saturate(segmentAlpha * g_Tint.a * saturate(In.segmentParams.z) * sideAlpha);

    const float2 flowUV = Build_FlowUV(In.texCoord);
    const float2 flow = 0 != g_EffectDistortionParams.z
                        ? Sample_FlowTexture(flowUV) * 2.f - 1.f
                        : float2(0.f, 0.f);

    const float lensCoverage = EffectDistortion_ComputeLensCoverage(trailCoverage, 1.f, g_EffectDistortionParams.y);
    const float refractionIntensity =
        g_EffectDistortionParams.x * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_REFRACTION_INTENSITY, lifeProgress);
    const float2 offset = EffectDistortion_ComputeScreenOffset(flow, refractionIntensity);
    Out.color = EffectDistortion_PackField(offset, lensCoverage);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(DistortionPass, RS_CullNone, DSS_DepthRead, BS_Blend, VS_MAIN, GS_MAIN, PS_MAIN)
}
