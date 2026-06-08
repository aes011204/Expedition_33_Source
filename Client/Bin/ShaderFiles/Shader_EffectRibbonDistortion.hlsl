#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Shader_EffectDistortionCommon.hlsli"

cbuffer cbEffectRibbonDistortion : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectRibbonAxisParams = { 0.f, 1.f, 0.f, 0.f }; // x: renderAxis, y: visibleLength, z: tilingDistance, w: smooth start tangent
    float4 g_EffectRibbonFallbackAxis = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectDistortionParams = { 0.f, 1.f, 0.f, 0.f }; // x: intensity, y: presence, z: useFlow, w: material time
    float4 g_DistortionScreenSize = { 1.f, 1.f, 0.f, 0.f };
    float2 g_FlowUVScale = { 1.f, 1.f };
    float2 g_FlowUVOffset = { 0.f, 0.f };
    float2 g_FlowUVScrollSpeed = { 0.f, 0.f };
    float g_FlowUVTilingMode = 0.f;
    float g_RibbonDistortionPadding = 0.f;
};

Texture2D g_FlowTexture : register(t3);

static const int RIBBON_RENDER_AXIS_CAMERA_UP = 0;
static const int RIBBON_RENDER_AXIS_VIEW_UP = 1;
static const int RIBBON_RENDER_AXIS_WORLD_UP = 2;
static const float kRibbonEdgeFade = 0.04f;

float3 NormalizeOrFallback(float3 value, float3 fallbackAxis)
{
    const float lengthSq = dot(value, value);
    return lengthSq > 0.000001f ? value * rsqrt(lengthSq) : fallbackAxis;
}

float3 Resolve_ViewUpAxis()
{
    return NormalizeOrFallback(g_ViewMatrixInverse[1].xyz, float3(0.f, 1.f, 0.f));
}

float3 Resolve_RibbonTangent(float3 previousPosition, float3 currentPosition, float3 nextPosition, float hasPreviousSample)
{
    const float3 forward = NormalizeOrFallback(nextPosition - currentPosition, float3(0.f, 0.f, 1.f));
    if (g_EffectRibbonAxisParams.w < 0.5f || hasPreviousSample < 0.5f)
        return forward;

    const float3 backward = NormalizeOrFallback(currentPosition - previousPosition, forward);
    return NormalizeOrFallback(backward + forward, forward);
}

float3 Resolve_RibbonSide(float3 currentPosition, float3 tangent)
{
    const float3 fallbackAxis = NormalizeOrFallback(g_EffectRibbonFallbackAxis.xyz, float3(1.f, 0.f, 0.f));
    const int renderAxis = (int)g_EffectRibbonAxisParams.x;

    float3 axis = NormalizeOrFallback(g_CamPosition.xyz - currentPosition, float3(0.f, 0.f, -1.f));
    if (renderAxis == RIBBON_RENDER_AXIS_VIEW_UP)
        axis = Resolve_ViewUpAxis();
    else if (renderAxis == RIBBON_RENDER_AXIS_WORLD_UP)
        axis = float3(0.f, 1.f, 0.f);

    float3 side = NormalizeOrFallback(cross(axis, tangent), fallbackAxis);
    if (abs(dot(side, tangent)) > 0.98f)
        side = NormalizeOrFallback(cross(Resolve_ViewUpAxis(), tangent), fallbackAxis);

    return side;
}

float2 Build_FlowUV(float2 rawUV)
{
    const float2 transformedUV = rawUV * g_FlowUVScale + g_FlowUVOffset + g_FlowUVScrollSpeed * g_EffectDistortionParams.w;
    return g_FlowUVTilingMode >= 2.5f ? rawUV : transformedUV;
}

float2 Sample_FlowTexture(float2 uv)
{
    return g_FlowUVTilingMode >= 0.5f && g_FlowUVTilingMode < 2.5f
           ? g_FlowTexture.Sample(LinearClampSampler, uv).rg
           : g_FlowTexture.Sample(LinearWrapSampler, uv).rg;
}

struct VS_IN
{
    float3 position : POSITION;
    float4 previousPosition : WORLD0;
    float4 currentPosition : WORLD1;
    float4 nextPosition : WORLD2;
    float2 lifeTime : TEXCOORD0;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD1;
    float4 segmentParams : TEXCOORD2; // x: life progress, y: width, z: alpha scale, w: hasPrevious
};

struct VS_OUT
{
    float3 previousPosition : POSITION;
    float3 currentPosition : TEXCOORD0;
    float3 nextPosition : TEXCOORD1;
    float2 lifeTime : TEXCOORD2;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD3;
    float4 segmentParams : TEXCOORD4;
};

struct GS_IN
{
    float3 previousPosition : POSITION;
    float3 currentPosition : TEXCOORD0;
    float3 nextPosition : TEXCOORD1;
    float2 lifeTime : TEXCOORD2;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD3;
    float4 segmentParams : TEXCOORD4;
};

struct GS_OUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float2 edgeCoord : TEXCOORD2;
    float4 color : COLOR0;
    float4 segmentParams : TEXCOORD3;
};

struct PS_IN
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float2 edgeCoord : TEXCOORD2;
    float4 color : COLOR0;
    float4 segmentParams : TEXCOORD3;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    Out.previousPosition = In.previousPosition.xyz;
    Out.currentPosition = In.currentPosition.xyz;
    Out.nextPosition = In.nextPosition.xyz;
    Out.lifeTime = In.lifeTime;
    Out.startColor = In.startColor;
    Out.endColor = In.endColor;
    Out.subUVRect = In.subUVRect;
    Out.segmentParams = In.segmentParams;

    return Out;
}

GS_OUT Build_RibbonVertex(
    float3 worldPosition,
    float2 texCoord,
    float2 edgeCoord,
    GS_IN input)
{
    const matrix viewProj = mul(g_ViewMatrix, g_ProjMatrix);

    GS_OUT Out = (GS_OUT)0;
    Out.position = mul(float4(worldPosition, 1.f), viewProj);
    Out.texCoord = texCoord;
    Out.lifeTime = input.lifeTime;
    Out.edgeCoord = edgeCoord;
    Out.color = lerp(input.startColor, input.endColor, saturate(input.segmentParams.x));
    Out.segmentParams = input.segmentParams;
    return Out;
}

[maxvertexcount(4)]
void GS_MAIN(point GS_IN In[1], inout TriangleStream<GS_OUT> OutStream)
{
    const float3 currentPosition = In[0].currentPosition;
    const float3 nextPosition = In[0].nextPosition;
    const float3 tangent = Resolve_RibbonTangent(
        In[0].previousPosition,
        currentPosition,
        nextPosition,
        In[0].segmentParams.w
    );
    const float3 side = Resolve_RibbonSide(currentPosition, tangent);
    const float halfWidth = max(In[0].segmentParams.y, 0.001f) * 0.5f;

    const float3 currentA = currentPosition - side * halfWidth;
    const float3 currentB = currentPosition + side * halfWidth;
    const float3 nextA = nextPosition - side * halfWidth;
    const float3 nextB = nextPosition + side * halfWidth;

    OutStream.Append(Build_RibbonVertex(currentA, In[0].subUVRect.xy, float2(0.f, 0.f), In[0]));
    OutStream.Append(Build_RibbonVertex(currentB, float2(In[0].subUVRect.z, In[0].subUVRect.y), float2(0.f, 1.f), In[0]));
    OutStream.Append(Build_RibbonVertex(nextA, float2(In[0].subUVRect.x, In[0].subUVRect.w), float2(1.f, 0.f), In[0]));
    OutStream.Append(Build_RibbonVertex(nextB, In[0].subUVRect.zw, float2(1.f, 1.f), In[0]));
}

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float sideAlpha =
        smoothstep(0.0f, kRibbonEdgeFade, In.edgeCoord.y) *
        smoothstep(0.0f, kRibbonEdgeFade, 1.0f - In.edgeCoord.y);
    const float trailCoverage = saturate(In.color.a * g_Tint.a * saturate(In.segmentParams.z) * sideAlpha);
    const float2 flowUV = Build_FlowUV(In.texCoord);
    const float2 flow = 0 != g_EffectDistortionParams.z
                        ? Sample_FlowTexture(flowUV) * 2.f - 1.f
                        : float2(0.f, 0.f);

    const float lensCoverage = EffectDistortion_ComputeLensCoverage(trailCoverage, 1.f, g_EffectDistortionParams.y);
    const float2 offset = EffectDistortion_ComputeScreenOffset(flow, g_EffectDistortionParams.x);
    Out.color = EffectDistortion_PackField(offset, lensCoverage);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(DistortionPass, RS_CullNone, DSS_DepthRead, BS_Blend, VS_MAIN, GS_MAIN, PS_MAIN)
}
