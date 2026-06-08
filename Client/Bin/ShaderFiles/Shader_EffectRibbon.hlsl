#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"

cbuffer cbEffectRibbonMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectRibbonParams = { 1.f, 1.f, 0.f, 0.f };     // x: intensity, y: opacityPower, z: alphaCutoff, w: materialTime
    float4 g_EffectRibbonAxisParams = { 0.f, 1.f, 0.f, 0.f }; // x: renderAxis, y: visibleLength, z: tilingDistance, w: reserved
    float4 g_EffectRibbonFallbackAxis = { 1.f, 0.f, 0.f, 0.f };
};

Texture2D g_MainTexture : register(t0);

static const int RIBBON_RENDER_AXIS_CAMERA_UP = 0;
static const int RIBBON_RENDER_AXIS_VIEW_UP = 1;
static const int RIBBON_RENDER_AXIS_WORLD_UP = 2;

struct VS_IN
{
    float3 position : POSITION;
    row_major float2x4 segmentPositions : WORLD;
    float2 lifeTime : TEXCOORD0;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD1;
    float4 segmentParams : TEXCOORD2; // x: life progress, y: width, z: alpha scale, w: reserved
};

struct VS_OUT
{
    float3 currentPosition : POSITION;
    float3 nextPosition : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
    float4 segmentParams : TEXCOORD3;
};

struct GS_IN
{
    float3 currentPosition : POSITION;
    float3 nextPosition : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
    float4 segmentParams : TEXCOORD3;
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

float3 NormalizeOrFallback(float3 value, float3 fallbackAxis)
{
    const float lengthSq = dot(value, value);
    return lengthSq > 0.000001f ? value * rsqrt(lengthSq) : fallbackAxis;
}

float3 Resolve_ViewUpAxis()
{
    return NormalizeOrFallback(g_ViewMatrixInverse[1].xyz, float3(0.f, 1.f, 0.f));
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

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    Out.currentPosition = In.segmentPositions[0].xyz;
    Out.nextPosition = In.segmentPositions[1].xyz;
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
    const float3 tangent = NormalizeOrFallback(nextPosition - currentPosition, float3(0.f, 0.f, 1.f));
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

float Resolve_RibbonCoverage(PS_IN In, float textureAlpha)
{
    const float opacityPower = max(g_EffectRibbonParams.y, 0.0001f);
    const float textureCoverage = pow(saturate(textureAlpha), opacityPower);
    const float sideAlpha =
        smoothstep(0.0f, 0.04f, In.edgeCoord.y) *
        smoothstep(0.0f, 0.04f, 1.0f - In.edgeCoord.y);

    return textureCoverage * sideAlpha * saturate(In.color.a * g_Tint.a * In.segmentParams.z);
}

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float4 tex = g_MainTexture.Sample(LinearWrapSampler, In.texCoord);
    const float coverage = Resolve_RibbonCoverage(In, tex.a);
    if (coverage < max(g_EffectRibbonParams.z, 0.0f))
        discard;

    const float intensity = max(g_EffectRibbonParams.x, 0.f);
    Out.color = float4(tex.rgb * In.color.rgb * g_Tint.rgb * intensity, coverage);
    return Out;
}

PS_OUT PS_ADDITIVE(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float4 tex = g_MainTexture.Sample(LinearWrapSampler, In.texCoord);
    const float coverage = Resolve_RibbonCoverage(In, tex.a);
    if (coverage < max(g_EffectRibbonParams.z, 0.0f))
        discard;

    const float intensity = max(g_EffectRibbonParams.x, 0.f);
    Out.color = float4(tex.rgb * In.color.rgb * g_Tint.rgb * intensity * coverage, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(AlphaBlendPass, RS_CullNone, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, GS_MAIN, PS_MAIN)
    PASS_RS_DS_BS_VGP(AdditivePass, RS_CullNone, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, GS_MAIN, PS_ADDITIVE)
}
