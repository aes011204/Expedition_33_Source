#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"

cbuffer cbCamera : register(b1)
{
    float4x4 g_ViewMatrix;
    float4x4 g_ProjMatrix;
    float4 g_vBillboardTarget;
};

Texture2D g_Texture : register(t0);

// 파티클 텍스처 샘플링용 기본 샘플러.
SamplerState DefaultSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

struct VS_IN
{
    float3 vPosition : POSITION;
    row_major float4x4 InstanceMatrix : WORLD;
    float2 vLifeTime : TEXCOORD0;
};

struct VS_OUT
{
    float4 vWorldPosition : POSITION;
    float2 vSize : TEXCOORD0;
    float2 vLifeTime : TEXCOORD1;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    const float4 vWorldPosition = mul(float4(In.vPosition, 1.f), In.InstanceMatrix);
    Out.vWorldPosition = vWorldPosition;
    Out.vSize = float2(length(In.InstanceMatrix[0].xyz), length(In.InstanceMatrix[1].xyz));
    Out.vLifeTime = In.vLifeTime;

    return Out;
}

struct GS_IN
{
    float4 vWorldPosition : POSITION;
    float2 vSize : TEXCOORD0;
    float2 vLifeTime : TEXCOORD1;
};

struct GS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexCoord : TEXCOORD0;
    float2 vLifeTime : TEXCOORD1;
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

    Out[1].vPosition = mul(float4(vParticlePosition + vRight + vUp, 1.f), ViewProjMatrix);
    Out[1].vTexCoord = float2(1.f, 0.f);
    Out[1].vLifeTime = In[0].vLifeTime;

    Out[2].vPosition = mul(float4(vParticlePosition - vRight - vUp, 1.f), ViewProjMatrix);
    Out[2].vTexCoord = float2(0.f, 1.f);
    Out[2].vLifeTime = In[0].vLifeTime;

    Out[3].vPosition = mul(float4(vParticlePosition + vRight - vUp, 1.f), ViewProjMatrix);
    Out[3].vTexCoord = float2(1.f, 1.f);
    Out[3].vLifeTime = In[0].vLifeTime;

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
};

struct PS_OUT
{
    float4 vColor : SV_TARGET0;
};

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    Out.vColor = g_Texture.Sample(DefaultSampler, In.vTexCoord);

    if (Out.vColor.a < 0.3f)
        discard;

    const float fLifeMax = max(In.vLifeTime.x, 0.0001f);
    const float fLifeRatio = saturate(1.f - In.vLifeTime.y / fLifeMax);
    Out.vColor.a *= fLifeRatio;

    return Out;
}

// @formatter:off
technique11 DefaultTechnique
{
    pass DefaultPass
    {
        SetRasterizerState(RS_CullNone);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = compile gs_5_0 GS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN();
    }
}
// @formatter:on
