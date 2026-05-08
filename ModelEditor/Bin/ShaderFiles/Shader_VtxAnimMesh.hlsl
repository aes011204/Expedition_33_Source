#include "Engine_Shader_Defines.hlsli"

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

vector g_vLightDir;
vector g_vLightDiffuse;
vector g_vLightAmbient;
vector g_vLightSpecular;

texture2D g_DiffuseTexture;
vector g_vMtrlAmbient = vector(0.3f, 0.3f, 0.3f, 1.f);
vector g_vMtrlSpecular = vector(1.f, 1.f, 1.f, 1.f);
vector g_vCamPosition;

cbuffer BoneTransforms : register(b1)
{
    matrix g_BoneMatrices[950];
};

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
    float3 vTangent : TANGENT;
    float3 vBinormal : BINORMAL;
    float2 vTexcoord : TEXCOORD0;
    uint4 vBlendIndex : BLENDINDEX;
    float4 vBlendWeight : BLENDWEIGHT;
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

    matrix BoneMatrix = g_BoneMatrices[In.vBlendIndex.x] * In.vBlendWeight.x +
        g_BoneMatrices[In.vBlendIndex.y] * In.vBlendWeight.y +
        g_BoneMatrices[In.vBlendIndex.z] * In.vBlendWeight.z +
        g_BoneMatrices[In.vBlendIndex.w] * In.vBlendWeight.w;

    vector vPosition = mul(float4(In.vPosition, 1.f), BoneMatrix);
    vector vNormal = mul(float4(In.vNormal, 0.f), BoneMatrix);

    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(vPosition, matWVP);
    Out.vNormal = normalize(mul(vNormal, g_WorldMatrix));

    Out.vTexcoord = In.vTexcoord;
    Out.vWorldPos = mul(vPosition, g_WorldMatrix);

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
    vector      vMtrlDiffuse = g_DiffuseTexture.Sample(DefaultSampler, In.vTexcoord);

    if (vMtrlDiffuse.a < 0.3f)
        discard;

    vector vShade = saturate(max(dot(normalize(g_vLightDir) * -1.f, In.vNormal), 0.f) + (g_vLightAmbient * g_vMtrlAmbient));

    vector vLook = In.vWorldPos - g_vCamPosition;
    vector vRelfect = reflect(normalize(g_vLightDir), In.vNormal);
    float fSpecular = pow(max(dot(normalize(vLook) * -1.f, normalize(vRelfect)), 0.f), 50.f);

    vector vSpecularColor = g_vLightSpecular * g_vMtrlSpecular * fSpecular;

    Out.vColor = g_vLightDiffuse * vMtrlDiffuse * vShade + vSpecularColor;
    Out.vColor.a = 1.f;

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
