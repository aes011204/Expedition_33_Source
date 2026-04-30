#include "Engine_Shader_Defines.hlsli"

/* Vertex Shader */
/* 1. 정점의 변환과정을 수행한다.(월드, 뷰, 투영 등) */
/* 2. 정점의 구성을 변형한다. */

/* float2, float3, float4 == vector */
/* float1x4, float4x4 == matrix */


/* 컨스턴트 테이블 */
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

vector g_vLightDir;
vector g_vLightDiffuse;
vector g_vLightAmbient;
vector g_vLightSpecular;

texture2D g_DiffuseTexture[2];
texture2D g_MaskTexture;
texture2D g_BrushTexture;
vector g_vMtrlAmbient = vector(0.3f, 0.3f, 0.3f, 1.f);
vector g_vMtrlSpecular = vector(1.f, 1.f, 1.f, 1.f);
vector g_vCamPosition;

float g_fBrushRange = 15.f;
vector g_vBrushPosition = vector(70.f, 0.f, 20.f, 1.f);

sampler DefaultSampler = sampler_state
{
    Filter = MIN_MAG_MIP_LINEAR;   
    AddressU = wrap;
    AddressV = wrap;
};



/* 이 쉐이더로 그린다라고 이야기한 정점버퍼의 정점을 꺼내서 받아온다 */
struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;    
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;
    float4 vWorldPos : TEXCOORD1;
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
    Out.vNormal = normalize(mul(float4(In.vNormal, 0.f), g_WorldMatrix));
    Out.vTexcoord = In.vTexcoord;
    Out.vWorldPos = mul(float4(In.vPosition, 1.f), g_WorldMatrix);
    
    return Out;
}

/* w나누기연산을 수행한다.-> 이 연산으로 이어질수 있는 이유 -> VS_OUT구조체의 위치 -> SV_ */
/* 뷰포트(윈도우좌표)로 변환한다. */
/* 래스터라이즈 -> 정점 세개로 감싸진 영역의 픽셀 정보를 생성한다 */

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float4 vNormal : NORMAL;
    float2 vTexcoord : TEXCOORD0;    
    float4 vWorldPos : TEXCOORD1;
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
    vector vSourMtrlDiffuse = g_DiffuseTexture[0].Sample(DefaultSampler, In.vTexcoord * 30.f);
    vector vDestMtrlDiffuse = g_DiffuseTexture[1].Sample(DefaultSampler, In.vTexcoord * 30.f);
    vector vMask = g_MaskTexture.Sample(DefaultSampler, In.vTexcoord);
    vector vBrush = 0.f;
    
    if (g_vBrushPosition.x - g_fBrushRange < In.vWorldPos.x && In.vWorldPos.x <= g_vBrushPosition.x + g_fBrushRange && 
        g_vBrushPosition.z - g_fBrushRange < In.vWorldPos.z && In.vWorldPos.z <= g_vBrushPosition.z + g_fBrushRange)
    {
        float2 vTexcoord;
        
        vTexcoord.x = (In.vWorldPos.x - (g_vBrushPosition.x - g_fBrushRange)) / (2.f * g_fBrushRange);
        vTexcoord.y = ((g_vBrushPosition.z + g_fBrushRange) - In.vWorldPos.z) / (2.f * g_fBrushRange);
        
        vBrush = g_BrushTexture.Sample(DefaultSampler, vTexcoord);
    }
    
    vector vMtrlDiffuse = vDestMtrlDiffuse * vMask + vSourMtrlDiffuse * (1.f - vMask) + vBrush;
    
    
    vector vShade = saturate(max(dot(normalize(g_vLightDir) * -1.f, In.vNormal), 0.f) + (g_vLightAmbient * g_vMtrlAmbient));
    
    vector vLook = In.vWorldPos - g_vCamPosition;
    vector vRelfect = reflect(normalize(g_vLightDir), In.vNormal);    
    float fSpecular = pow(max(dot(normalize(vLook) * -1.f, normalize(vRelfect)), 0.f), 50.f);
    
    vector vSpecularColor = g_vLightSpecular * g_vMtrlSpecular * fSpecular;
    
    Out.vColor = g_vLightDiffuse * vMtrlDiffuse * vShade + vSpecularColor;
    
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
        PixelShader = compile ps_5_0 PS_MAIN();
    }
}

//    pass DefaultPass1
//    {
//        VertexShader = compile vs_5_0 VS_MAIN1();
//        PixelShader = compile ps_5_0 PS_MAIN1();

//    }
//}
