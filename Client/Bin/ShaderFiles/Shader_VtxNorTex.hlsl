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
Texture2D g_Layer0, g_Layer1, g_Layer2, g_Layer3, g_Layer4, g_Layer5, g_Layer6;
Texture2D g_Weight0, g_Weight1, g_Weight2, g_Weight3, g_Weight4, g_Weight5, g_Weight6;
Texture2D g_Normal0, g_Normal1, g_Normal2, g_Normal3, g_Normal4, g_Normal5, g_Normal6;
Texture2D g_Roughness0, g_Roughness1, g_Roughness2, g_Roughness3, g_Roughness4, g_Roughness5, g_Roughness6;

cbuffer LayerParams : register(b1)
{
    float4 g_Tiling[7];
    float4 g_GroundTint[7];
    float4 g_LayerControl[7];
    float4 g_AlbedoControl[7];
};

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
    vector Material : SV_TARGET3; // r=AO, g=Roughness, b=Metalness
    vector Emissive : SV_TARGET4;
};

PS_OUT PS_Default(PS_IN In)
{
    PS_OUT Out;
    Out.Diffuse = float4(1.f, 1.f, 1.f, 1.f);
    Out.Normal = vector(In.Normal.xyz * 0.5f + 0.5f, 1.f);
    Out.Depth = vector(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = vector(1.f, 0.8f, 0.f, 0.f);
    Out.Emissive = vector(0.f, 0.f, 0.f, 0.f);
    
    return Out;
}

float3 ApplyAlbedoControl(float3 color, float4 ctrl)
{
    float lum = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = lerp(float3(lum, lum, lum), color, ctrl.x);
    color *= ctrl.y;
    color = (color - 0.5f) * ctrl.z + 0.5f;
    return saturate(color);
}

float3 SampleLandscapeWorldNormal(Texture2D normalTex, float2 uv, float strength, float3x3 tbn)
{
    float2 normalXY = normalTex.Sample(LinearWrapSampler, uv).rg * 2.f - 1.f;
    normalXY *= strength;

    float normalZ = sqrt(saturate(1.f - dot(normalXY, normalXY)));
    float3 normalTS = normalize(float3(normalXY, normalZ));

    return normalize(mul(normalTS, tbn));
}

PS_OUT PS_Layer(PS_IN In)
{
    PS_OUT Out;

    float3 ddxP = ddx(In.WorldPos.xyz);
    float3 ddyP = ddy(In.WorldPos.xyz);
    float3x3 TBN = float3x3(normalize(ddxP), normalize(ddyP), normalize(In.Normal.xyz));

    float w[7];
    w[0] = g_Weight0.Sample(LinearClampSampler, In.Texcoord).r;
    w[1] = g_Weight1.Sample(LinearClampSampler, In.Texcoord).r;
    w[2] = g_Weight2.Sample(LinearClampSampler, In.Texcoord).r;
    w[3] = g_Weight3.Sample(LinearClampSampler, In.Texcoord).r;
    w[4] = g_Weight4.Sample(LinearClampSampler, In.Texcoord).r;
    w[5] = g_Weight5.Sample(LinearClampSampler, In.Texcoord).r;
    w[6] = g_Weight6.Sample(LinearClampSampler, In.Texcoord).r;

    float4 blendedColor = float4(0.f, 0.f, 0.f, 0.f);
    float3 blendedNormal = float3(0.f, 0.f, 0.f);
    float blendedRough = 0.f;
    float blendedMetallic = 0.f;
    float totalWeight = 0.f;

    // Layer0
    float2 uv0 = In.Texcoord * g_Tiling[0].xy;
    float4 col0 = g_Layer0.Sample(LinearWrapSampler, uv0);
    col0.rgb = ApplyAlbedoControl(col0.rgb, g_AlbedoControl[0]);
    blendedColor += col0 * g_GroundTint[0];// * w[0];
    blendedNormal += SampleLandscapeWorldNormal(g_Normal0, uv0, g_LayerControl[0].x, TBN) * w[0];
    blendedRough += g_Roughness0.Sample(LinearWrapSampler, uv0).r * g_LayerControl[0].y * w[0];
    blendedMetallic += g_LayerControl[0].w * w[0];
    totalWeight += w[0];
    
    // Layer1
    float2 uv1 = In.Texcoord * g_Tiling[1].xy;
    float4 col1 = g_Layer1.Sample(LinearWrapSampler, uv1);
    col1.rgb = ApplyAlbedoControl(col1.rgb, g_AlbedoControl[1]);
    blendedColor += col1 * g_GroundTint[1] * w[1];
    blendedNormal += SampleLandscapeWorldNormal(g_Normal1, uv1, g_LayerControl[1].x, TBN) * w[1];
    blendedRough += g_Roughness1.Sample(LinearWrapSampler, uv1).r * g_LayerControl[1].y * w[1];
    blendedMetallic += g_LayerControl[1].w * w[1];
    totalWeight += w[1];
    
    // Layer2
    float2 uv2 = In.Texcoord * g_Tiling[2].xy;
    float4 col2 = g_Layer2.Sample(LinearWrapSampler, uv2);
    col2.rgb = ApplyAlbedoControl(col2.rgb, g_AlbedoControl[2]);
    blendedColor += col2 * g_GroundTint[2] * w[2];
    blendedNormal += SampleLandscapeWorldNormal(g_Normal2, uv2, g_LayerControl[2].x, TBN) * w[2];
    blendedRough += g_Roughness2.Sample(LinearWrapSampler, uv2).r * g_LayerControl[2].y * w[2];
    blendedMetallic += g_LayerControl[2].w * w[2];
    totalWeight += w[2];
    
    // Layer3
    float2 uv3 = In.Texcoord * g_Tiling[3].xy;
    float4 col3 = g_Layer3.Sample(LinearWrapSampler, uv3);
    col3.rgb = ApplyAlbedoControl(col3.rgb, g_AlbedoControl[3]);
    blendedColor += col3 * g_GroundTint[3] * w[3];
    blendedNormal += SampleLandscapeWorldNormal(g_Normal3, uv3, g_LayerControl[3].x, TBN) * w[3];
    blendedRough += g_Roughness3.Sample(LinearWrapSampler, uv3).r * g_LayerControl[3].y * w[3];
    blendedMetallic += g_LayerControl[3].w * w[3];
    totalWeight += w[3];
    
    // Layer4
    float2 uv4 = In.Texcoord * g_Tiling[4].xy;
    float4 col4 = g_Layer4.Sample(LinearWrapSampler, uv4);
    col4.rgb = ApplyAlbedoControl(col4.rgb, g_AlbedoControl[4]);
    blendedColor += col4 * g_GroundTint[4] * w[4];
    blendedNormal += SampleLandscapeWorldNormal(g_Normal4, uv4, g_LayerControl[4].x, TBN) * w[4];
    blendedRough += g_Roughness4.Sample(LinearWrapSampler, uv4).r * g_LayerControl[4].y * w[4];
    blendedMetallic += g_LayerControl[4].w * w[4];
    totalWeight += w[4];
    
    // Layer 5
    float2 uv5 = In.Texcoord * g_Tiling[5].xy;
    float4 col5 = g_Layer5.Sample(LinearWrapSampler, uv5);
    col5.rgb = ApplyAlbedoControl(col5.rgb, g_AlbedoControl[5]);
    blendedColor += col5 * g_GroundTint[5] * w[5];
    blendedNormal += SampleLandscapeWorldNormal(g_Normal5, uv5, g_LayerControl[5].x, TBN) * w[5];
    blendedRough += g_Roughness5.Sample(LinearWrapSampler, uv5).r * g_LayerControl[5].y * w[5];
    blendedMetallic += g_LayerControl[5].w * w[5];
    totalWeight += w[5];
    
    // Layer 6
    float2 uv6 = In.Texcoord * g_Tiling[6].xy;
    float4 col6 = g_Layer6.Sample(LinearWrapSampler, uv6);
    col6.rgb = ApplyAlbedoControl(col6.rgb, g_AlbedoControl[6]);
    blendedColor += col6 * g_GroundTint[6] * w[6];
    blendedNormal += SampleLandscapeWorldNormal(g_Normal6, uv6, g_LayerControl[6].x, TBN) * w[6];
    blendedRough += g_Roughness6.Sample(LinearWrapSampler, uv6).r * g_LayerControl[6].y * w[6];
    blendedMetallic += g_LayerControl[6].w * w[6];
    totalWeight += w[6];
    
    if (totalWeight > 0.f)
    {
        blendedColor /= totalWeight;
        blendedRough /= totalWeight;
        blendedMetallic /= totalWeight;
    }
    
    blendedNormal = normalize(blendedNormal);

    Out.Diffuse = float4(blendedColor.rgb, 1.f);
    Out.Normal = float4(blendedNormal * 0.5f + 0.5f, 1.f);
    Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
    Out.Material = float4(1.f, saturate(blendedRough), saturate(blendedMetallic), 0.f);
    Out.Emissive = float4(0.f, 0.f, 0.f, 0.f);
    
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DefaultPass, RS_Default, DSS_Default, BS_Default, VS_Default, PS_Default) // 0
    PASS_RS_DS_BS_VP(LayerPass, RS_Default, DSS_Default, BS_Default, VS_Default, PS_Layer) // 1
}
