#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"

// Matrix & Camera

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
float4 g_CamPosition;
float g_Far;

// Textures

Texture2D g_LargeWaveNormal;
Texture2D g_SmallWaveNormal;
Texture2D g_SeafoamTexture;
Texture2D g_DepthTexture;
TextureCube g_ReflectionCubemap;

// Water Contant Buffer

cbuffer WaterBuffer : register(b13)
{
    float g_LargeWaveScale;
    float g_LargeWaveSpeed;
    float g_LargeWaveAmplify;
    float g_SmallWaveScale;

    float g_SmallWaveSpeed;
    float g_SmallWaveAmplify;
    float g_SeafoamScale;
    float g_SeafoamSpeed;

    float g_SeafoamHeightPower;
    float g_SeafoamHeightMultiply;
    float g_FoamDistortion;
    float g_DepthFade;

    float g_CameraDistanceFade;
    float g_ReflectionAmount;
    float g_LuminanceBias;
    float g_WaterElapsedTime;

    float4 g_WaterColor;
    float4 g_WaterFresnelColor;
};

// VTX

struct VS_IN
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 Texcoord : TEXCOORD;
};

struct VS_OUT
{
    float4 Position : SV_Position;
    float3 WorldPos : TEXCOORD0;
    float4 ProjPos : TEXCOORD1;
    float3 WorldNormal : TEXCOORD2;
};

VS_OUT VS_Water(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    float4x4 matWVP = mul(matWV, g_ProjMatrix);

    float4 worldPos = mul(float4(In.Position, 1.f), g_WorldMatrix);
    Out.Position = mul(float4(In.Position, 1.f), matWVP);
    Out.WorldPos = worldPos.xyz;
    Out.ProjPos = Out.Position;
    Out.WorldNormal = normalize(mul(float4(In.Normal, 0.f), g_WorldMatrix).xyz);

    return Out;
}

// PS

struct PS_IN
{
    float4 Position : SV_Position;
    float3 WorldPos : TEXCOORD0;
    float4 ProjPos : TEXCOORD1;
    float3 WorldNormal : TEXCOORD2;
};

struct PS_OUT
{
    float4 Color : SV_Target0;
};

float3 Motion4WayChaos_Normal(Texture2D normalTex, float2 uv, float speed, float time)
{
    float t = time * speed;
    float3 n0 = normalTex.Sample(LinearWrapSampler, uv + float2(t, t * 0.5f)).rgb;
    float3 n1 = normalTex.Sample(LinearWrapSampler, uv * float2(-1.f, 1.f) + float2(-t * 0.3f, t * 0.7f)).rgb;
    float3 n2 = normalTex.Sample(LinearWrapSampler, uv * float2(1.f, -1.f) + float2(t * 0.6f, -t * 0.4f)).rgb;
    float3 n3 = normalTex.Sample(LinearWrapSampler, -uv + float2(-t * 0.8f, t * 0.2f)).rgb;
    return (n0 + n1 + n2 + n3) * 0.25f;
}

PS_OUT PS_Water(PS_IN In)
{
    PS_OUT Out;

    float2 worldUV = In.WorldPos.xz * 0.01f;

    float3 largeN = Motion4WayChaos_Normal(g_LargeWaveNormal, worldUV / g_LargeWaveScale, g_LargeWaveSpeed, g_WaterElapsedTime);
    float3 smallN = Motion4WayChaos_Normal(g_SmallWaveNormal, worldUV / g_SmallWaveScale, g_SmallWaveSpeed, g_WaterElapsedTime);

    largeN = largeN * 2.f - 1.f;
    smallN = smallN * 2.f - 1.f;
    float3 blendedN = normalize(largeN * g_LargeWaveAmplify + smallN * g_SmallWaveAmplify);

    // Fresnel
    float3 viewDir = normalize(g_CamPosition.xyz - In.WorldPos);
    float fresnel = pow(1.f - saturate(dot(viewDir, blendedN)), 2.f);

    float3 waterColor = lerp(g_WaterColor.rgb, g_WaterFresnelColor.rgb, fresnel);

    // reflect
    float3 reflectDir = reflect(-viewDir, blendedN);
    float3 reflectColor = g_ReflectionCubemap.Sample(LinearWrapSampler, reflectDir).rgb;
    waterColor = lerp(waterColor, reflectColor, fresnel * g_ReflectionAmount);

    // Seafoam
    float2 foamUV = worldUV / g_SeafoamScale + blendedN.xz * g_FoamDistortion;
    float t = g_WaterElapsedTime * g_SeafoamSpeed;
    float4 foam = g_SeafoamTexture.Sample(LinearWrapSampler, foamUV + float2(t, t * 0.5f));
    float foamMask = pow(saturate(blendedN.y * g_SeafoamHeightMultiply), g_SeafoamHeightPower);
    waterColor = lerp(waterColor, foam.rgb, foam.a * foamMask);

    // Depth Fade
    float2 screenUV = (In.ProjPos.xy / In.ProjPos.w) * float2(0.5f, -0.5f) + 0.5f;
    float sceneDepthW = g_DepthTexture.Sample(PointClampSampler, screenUV).y * g_Far;
    float depthDiff = sceneDepthW - In.ProjPos.w;
    float depthAlpha = saturate(depthDiff / max(g_DepthFade, 1e-6));
    depthAlpha = lerp(1.f, depthAlpha, step(1e-6, g_DepthFade));

    // Cam Distance Fade
    float camDist = length(g_CamPosition.xyz - In.WorldPos);
    float useFade = step(1e-6, g_CameraDistanceFade);
    float distAlpha = lerp(1.f, saturate(1.f - camDist / max(g_CameraDistanceFade, 1e-6)), useFade);

    float finalAlpha = saturate(depthAlpha * distAlpha + fresnel * 0.5f);

    waterColor = saturate(waterColor + g_LuminanceBias);

    Out.Color = float4(waterColor, finalAlpha);
    return Out;
}

// teq
technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(WaterPass, RS_CullNone, DSS_DepthRead, BS_AlphaBlend, VS_Water, PS_Water)
}
