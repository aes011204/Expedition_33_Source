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
};

struct VS_OUT
{
    float3 center : POSITION;
    float3 bladeAxis : TEXCOORD0;
    float width : TEXCOORD1;
    float2 lifeTime : TEXCOORD2;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD3;
    float bladeLength : TEXCOORD4;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    Out.center = In.InstanceMatrix[3].xyz;
    Out.bladeAxis = In.InstanceMatrix[0].xyz;
    Out.width = max(abs(In.InstanceMatrix[1].y), 0.001f);
    Out.bladeLength = max(length(In.InstanceMatrix[0].xyz), 0.001f);
    Out.lifeTime = In.lifeTime;
    Out.startColor = In.startColor;
    Out.endColor = In.endColor;
    Out.subUVRect = In.subUVRect;

    return Out;
}

struct GS_IN
{
    float3 center : POSITION;
    float3 bladeAxis : TEXCOORD0;
    float width : TEXCOORD1;
    float2 lifeTime : TEXCOORD2;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD3;
    float bladeLength : TEXCOORD4;
};

struct GS_OUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
};

[maxvertexcount(4)]
void GS_MAIN(point GS_IN In[1], inout TriangleStream<GS_OUT> OutStream)
{
    const float3 axis = length(In[0].bladeAxis) < 0.0001f ? float3(0.f, 1.f, 0.f) : normalize(In[0].bladeAxis);
    float3 cameraDir = g_CamPosition.xyz - In[0].center;
    cameraDir = length(cameraDir) < 0.0001f ? float3(0.f, 0.f, 1.f) : normalize(cameraDir);
    float3 side = cross(cameraDir, axis);
    side = length(side) < 0.0001f ? float3(1.f, 0.f, 0.f) : normalize(side);

    const float3 halfAxis = axis * In[0].bladeLength * 0.5f;
    const float3 halfSide = side * In[0].width * 0.5f;
    const matrix viewProj = mul(g_ViewMatrix, g_ProjMatrix);

    GS_OUT Out;
    Out.lifeTime = In[0].lifeTime;
    Out.startColor = In[0].startColor;
    Out.endColor = In[0].endColor;

    Out.position = mul(float4(In[0].center - halfAxis - halfSide, 1.f), viewProj);
    Out.texCoord = In[0].subUVRect.xy;
    OutStream.Append(Out);

    Out.position = mul(float4(In[0].center + halfAxis - halfSide, 1.f), viewProj);
    Out.texCoord = float2(In[0].subUVRect.z, In[0].subUVRect.y);
    OutStream.Append(Out);

    Out.position = mul(float4(In[0].center - halfAxis + halfSide, 1.f), viewProj);
    Out.texCoord = float2(In[0].subUVRect.x, In[0].subUVRect.w);
    OutStream.Append(Out);

    Out.position = mul(float4(In[0].center + halfAxis + halfSide, 1.f), viewProj);
    Out.texCoord = In[0].subUVRect.zw;
    OutStream.Append(Out);
}

struct PS_IN
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
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
    const float4 color = lerp(In.startColor, In.endColor, lifeProgress) * g_Tint * tex;
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
