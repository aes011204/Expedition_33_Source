#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"

// TODO: Trail material 확장 시 noiseTexture/noiseStrength를 실제 strip opacity/distortion 해석에 연결한다.
// Phase3 v0에서는 main texture, tint, blend만 소비하고 noise 계열은 authoring 값 보존용으로 둔다.

float4 g_Tint = float4(1.f, 1.f, 1.f, 1.f);
Texture2D g_Texture : register(t0);

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
    const float3 halfWidth = (tipPosition - basePosition) * 0.5f * saturate(widthScale);
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

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float4 tex = g_Texture.Sample(LinearClampSampler, In.texCoord);
    const float sideFade = max(In.segmentParams.w, 0.0001f);
    const float sideAlpha =
        smoothstep(0.0f, sideFade, In.edgeCoord.y) *
        smoothstep(0.0f, sideFade, 1.0f - In.edgeCoord.y);
    float4 color = lerp(In.startColor, In.endColor, lifeProgress) * g_Tint * tex;

    color.w *= saturate(In.segmentParams.z) * sideAlpha;
    if (color.w < 0.01f)
        discard;

    Out.color = color;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(AlphaBlendPass, RS_CullNone, DSS_DepthRead, BS_AlphaBlend, VS_MAIN, GS_MAIN, PS_MAIN) // 0
    PASS_RS_DS_BS_VGP(AdditivePass, RS_CullNone, DSS_DepthRead, BS_Blend, VS_MAIN, GS_MAIN, PS_MAIN)        // 1
}
