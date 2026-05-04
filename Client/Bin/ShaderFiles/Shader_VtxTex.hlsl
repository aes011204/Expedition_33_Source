#include "Engine_Shader_Defines.hlsli"

/* Vertex Shader */
/* 1. ������ ��ȯ������ �����Ѵ�.(����, ��, ���� ��) */
/* 2. ������ ������ �����Ѵ�. */

/* float2, float3, float4 == vector */
/* float1x4, float4x4 == matrix */


/* ������Ʈ ���̺� */
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
texture2D g_Texture;

sampler DefaultSampler = sampler_state
{
    Filter = MIN_MAG_MIP_LINEAR;    
};



/* �� ���̴��� �׸��ٶ�� �̾߱��� ���������� ������ ������ �޾ƿ´� */
struct VS_IN
{
    float3 vPosition : POSITION;
    float2 vTexcoord : TEXCOORD0;    
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;
    
    /* ������ ��ġ * ���� * �� * ���� */
    float4x4 matWV, matWVP;
    
    /* mul : ��ĳ����� ���ϱ⿬���� �����Ѵ�. */ 
    /* XMVector3TransformCoord : ���Ϳ� ����� ���ϱ⿬���� �����ϰ� w������.*/
    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);
    
    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP); 
    Out.vTexcoord = In.vTexcoord;
    
    return Out;
}

/* w�����⿬���� �����Ѵ�.-> �� �������� �̾����� �ִ� ���� -> VS_OUT����ü�� ��ġ -> SV_ */
/* ����Ʈ(��������ǥ)�� ��ȯ�Ѵ�. */
/* �����Ͷ����� -> ���� ������ ������ ������ �ȼ� ������ �����Ѵ� */

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;    
};

struct PS_OUT
{
    vector vColor : SV_TARGET0;
};

/* Pixel Shader -> �ȼ��� ���� �����Ѵ�. */

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out;
    
    // Out.vColor = float4(In.vTexcoord.y, In.vTexcoord.y, In.vTexcoord.y, 1.f);
    Out.vColor = g_Texture.Sample(DefaultSampler, In.vTexcoord);
    Out.vColor.gb = Out.vColor.r;
    
    return Out;

}

technique11 DefaultTechnique
{
    pass DefaultPass
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_MAIN();
    }
}
