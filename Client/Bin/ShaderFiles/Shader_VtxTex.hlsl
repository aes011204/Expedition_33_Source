#include "Engine_Shader_Defines.hlsli"

/* Vertex Shader */
/* 1. 정점의 변환과정을 수행한다.(월드, 뷰, 투영 등) */
/* 2. 정점의 구성을 변형한다. */

/* float2, float3, float4 == vector */
/* float1x4, float4x4 == matrix */


/* 컨스턴트 테이블 */
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
texture2D g_Texture;

sampler DefaultSampler = sampler_state
{
    Filter = MIN_MAG_MIP_LINEAR;    
};



/* 이 쉐이더로 그린다라고 이야기한 정점버퍼의 정점을 꺼내서 받아온다 */
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
    
    /* 정점의 위치 * 월드 * 뷰 * 투영 */
    float4x4 matWV, matWVP;
    
    /* mul : 행렬끼리의 곱하기연산을 수행한다. */ 
    /* XMVector3TransformCoord : 벡터와 행렬의 곱하기연산을 수행하고 w나눈다.*/
    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);
    
    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP); 
    Out.vTexcoord = In.vTexcoord;
    
    return Out;
}

/* w나누기연산을 수행한다.-> 이 연산으로 이어질수 있는 이유 -> VS_OUT구조체의 위치 -> SV_ */
/* 뷰포트(윈도우좌표)로 변환한다. */
/* 래스터라이즈 -> 정점 세개로 감싸진 영역의 픽셀 정보를 생성한다 */

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;    
};

struct PS_OUT
{
    vector vColor : SV_TARGET0;
};

/* Pixel Shader -> 픽셀의 색을 결정한다. */

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
