#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"

//==================================================
// MATRIX
//==================================================
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

//==================================================
// CAMERA
//==================================================
float g_Far = 0.f;

//==================================================
// TEXTURE
//==================================================
//Texture2D g_DiffuseTexture;
Texture2D g_Layer0, g_Layer1, g_Layer2, g_Layer3;
Texture2D g_Weight0, g_Weight1, g_Weight2, g_Weight3;

vector g_vBrushPosition = vector(70.f, 0.f, 20.f, 1.f);

//==================================================
// VS
//==================================================
struct VS_IN
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 Texcoord : TEXCOORD0;    
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float4 Normal   : NORMAL;
    float2 Texcoord : TEXCOORD0;
    float4 WorldPos : TEXCOORD1;
    float4 ProjPos  : TEXCOORD2;

};

VS_OUT VS_Default(VS_IN In)
{
    VS_OUT Out;
    
    /* 정점의 위치 * 월드 * 뷰 * 투영 */
    float4x4 matWV, matWVP;
    
    /* mul : 행렬끼리의 곱하기연산을 수행한다. */ 
    /* XMVector3TransformCoord : 벡터와 행렬의 곱하기연산을 수행하고 w나눈다.*/
    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);
    
    Out.Position = mul(float4(In.Position, 1.f), matWVP); 
    Out.Normal = normalize(mul(float4(In.Normal, 0.f), g_WorldMatrix));
    Out.Texcoord = In.Texcoord;
    Out.WorldPos = mul(float4(In.Position, 1.f), g_WorldMatrix);
    Out.ProjPos = Out.Position;

    return Out;
}

//==================================================
// PS
//==================================================
struct PS_IN
{
    float4 Position : SV_POSITION;
    float4 Normal   : NORMAL;
    float2 Texcoord : TEXCOORD0;    
    float4 WorldPos : TEXCOORD1;
    float4 ProjPos  : TEXCOORD2;
};

struct PS_OUT
{
    vector Diffuse  : SV_TARGET0;
    vector Normal   : SV_TARGET1;
    vector Depth    : SV_TARGET2;
};

PS_OUT PS_Default(PS_IN In)
{
    PS_OUT Out;
    Out.Diffuse = float4(1.f, 1.f, 1.f, 1.f);
    Out.Normal = vector(In.Normal.xyz * 0.5f + 0.5f, 1.f);
    Out.Depth = vector(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    
    return Out;
}

PS_OUT PS_Layer(PS_IN In)
{
    PS_OUT Out;
    float2 tiledUV = In.Texcoord * 30.f;
    float4 color = g_Layer0.Sample(LinearWrapSampler, tiledUV) * g_Weight0.Sample(LinearClampSampler, In.Texcoord).r
                 + g_Layer1.Sample(LinearWrapSampler, tiledUV) * g_Weight1.Sample(LinearClampSampler, In.Texcoord).r
                 + g_Layer2.Sample(LinearWrapSampler, tiledUV) * g_Weight2.Sample(LinearClampSampler, In.Texcoord).r
                 + g_Layer3.Sample(LinearWrapSampler, tiledUV) * g_Weight3.Sample(LinearClampSampler, In.Texcoord).r;
    Out.Diffuse = vector(color.rgb, 1.f);
    Out.Normal = vector(In.Normal.xyz * 0.5f + 0.5f, 1.f);
    Out.Depth = vector(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DefaultPass, RS_Default, DSS_Default, BS_Default, VS_Default, PS_Default) // 0
    PASS_RS_DS_BS_VP(LayerPass, RS_Default, DSS_Default, BS_Default, VS_Default, PS_Layer) // 1
}
