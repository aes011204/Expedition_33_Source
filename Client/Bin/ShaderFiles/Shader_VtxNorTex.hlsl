#include "Engine_Shader_Defines.hlsli"

//==================================================
// MATRIX
//==================================================
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

//==================================================
// LIGHT
//==================================================
vector g_vLightDir;
vector g_vLightDiffuse;
vector g_vLightAmbient;
vector g_vLightSpecular;

//==================================================
// TEXTURE
//==================================================
texture2D g_DiffuseTexture[2];
texture2D g_MaskTexture;
texture2D g_BrushTexture;

vector g_vMtrlAmbient = vector(0.3f, 0.3f, 0.3f, 1.f);
vector g_vMtrlSpecular = vector(1.f, 1.f, 1.f, 1.f);
vector g_vCamPosition;

float g_fBrushRange = 15.f;
vector g_vBrushPosition = vector(70.f, 0.f, 20.f, 1.f);

//==================================================
// SAMPLER
//==================================================
sampler DefaultSampler = sampler_state
{
    Filter = MIN_MAG_MIP_LINEAR;   
    AddressU = wrap;
    AddressV = wrap;
};

//==================================================
// VS
//==================================================
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

//==================================================
// PS
//==================================================
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

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out;
    
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
