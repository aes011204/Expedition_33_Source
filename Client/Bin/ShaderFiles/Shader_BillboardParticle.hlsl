#include "Engine_Shader_Defines.hlsli"

float4x4 g_ViewMatrix, g_ProjMatrix;
texture2D g_Texture;

// 파티클 텍스처 샘플링용 기본 샘플러.
sampler DefaultSampler = sampler_state
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = clamp;
    AddressV = clamp;
};

struct VS_IN
{
    float3 vPosition : POSITION;
    float4 vRight : WORLD0;
    float4 vUp : WORLD1;
    float4 vLook : WORLD2;
    float4 vTranslation : WORLD3;
    float2 vLifeTime : TEXCOORD0;
};

struct VS_OUT
{
    float3 vCenter : POSITION;
    float2 vSize : SIZE0;
    float2 vLifeTime : TEXCOORD0;
};

struct GS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
    float fLifeRatio : TEXCOORD1;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    // instance world payload에서 billboard 중심과 크기를 추출한다.
    Out.vCenter = In.vTranslation.xyz + In.vPosition;
    Out.vSize = float2(length(In.vRight.xyz), length(In.vUp.xyz));
    Out.vLifeTime = In.vLifeTime;

    return Out;
}

[maxvertexcount(4)]
void GS_MAIN(point VS_OUT In[1], inout TriangleStream<GS_OUT> OutStream)
{
    // view matrix에서 카메라 기준 right/up 축을 가져와 quad를 만든다.
    float3 vCamRight = normalize(float3(g_ViewMatrix._11, g_ViewMatrix._21, g_ViewMatrix._31));
    float3 vCamUp = normalize(float3(g_ViewMatrix._12, g_ViewMatrix._22, g_ViewMatrix._32));

    float2 vHalfSize = In[0].vSize * 0.5f;
    float fLifeRatio = 0.f;

    if (In[0].vLifeTime.x > 0.f)
        fLifeRatio = saturate(In[0].vLifeTime.y / In[0].vLifeTime.x);

    float3 vCenter = In[0].vCenter;

    float3 vPositions[4] =
    {
        vCenter + (-vCamRight * vHalfSize.x) + (vCamUp * vHalfSize.y),
        vCenter + (vCamRight * vHalfSize.x) + (vCamUp * vHalfSize.y),
        vCenter + (-vCamRight * vHalfSize.x) + (-vCamUp * vHalfSize.y),
        vCenter + (vCamRight * vHalfSize.x) + (-vCamUp * vHalfSize.y)
    };

    float2 vTexcoords[4] =
    {
        float2(0.f, 0.f),
        float2(1.f, 0.f),
        float2(0.f, 1.f),
        float2(1.f, 1.f)
    };

    for (int i = 0; i < 4; ++i)
    {
        GS_OUT Out;

        float4 vViewPosition = mul(float4(vPositions[i], 1.f), g_ViewMatrix);
        Out.vPosition = mul(vViewPosition, g_ProjMatrix);
        Out.vTexcoord = vTexcoords[i];
        Out.fLifeRatio = fLifeRatio;

        OutStream.Append(Out);
    }
}

struct PS_OUT
{
    vector vColor : SV_TARGET0;
};

PS_OUT PS_MAIN(GS_OUT In)
{
    PS_OUT Out;

    vector vColor = g_Texture.Sample(DefaultSampler, In.vTexcoord);

    // texture alpha와 lifetime fade를 함께 적용한다.
    clip(vColor.a - 0.1f);
    vColor.a *= 1.f - In.fLifeRatio;
    clip(vColor.a - 0.01f);

    Out.vColor = vColor;
    return Out;
}

technique11 DefaultTechnique
{
    pass DefaultPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_AlphaBlend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = compile gs_5_0 GS_MAIN();
        PixelShader = compile ps_5_0 PS_MAIN();
    }
}
