#ifndef ENGINE_SHADER_DEFERRED_HLSL
#define ENGINE_SHADER_DEFERRED_HLSL

#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Utils.hlsl"

//==================================================
// TEXTURE
//==================================================
Texture2D g_Texture;
Texture2D g_DiffuseTexture;
Texture2D g_NormalTexture;
Texture2D g_ShadeTexture;
Texture2D g_SpecularTexture;
Texture2D g_DepthTexture;
Texture2D g_LightDepthTexture;
Texture2D g_MaterialTexture;
Texture2D g_EmissiveTexture;
Texture2D g_StaticLightDepthTexture;
Texture2D g_SSAOTexture;
Texture2D g_SSAOBlurTexture;

//==================================================
// LIGHT
//==================================================
vector g_MtrlAmbient = float4(1.f, 1.f, 1.f, 1.f);
vector g_MtrlSpecular = float4(1.f, 1.f, 1.f, 1.f);



float4 Reconstruct_WorldPos(float2 Texcoord)
{
    vector DepthDesc = g_DepthTexture.Sample(LinearWrapSampler, Texcoord);
    float fViewZ = DepthDesc.y * g_DeferredFar;

    float4 WorldPos;

    // -> ndc space
    WorldPos.x = Texcoord.x * 2.f - 1.f;
    WorldPos.y = Texcoord.y * -2.f + 1.f;
    WorldPos.z = DepthDesc.x;
    WorldPos.w = 1.f;

    // -> clip space
    WorldPos *= fViewZ;

    // -> view space
    WorldPos = mul(WorldPos, g_DeferredProjMatrixInverse);

    // -> world space
    WorldPos = mul(WorldPos, g_DeferredViewMatrixInverse);

    return WorldPos;
}

float SampleShadowAmount(Texture2D shadowTexture, float4 worldPos, float4x4 lightView, float4x4 lightProj, float bias)
{
    float4 shadowPos = mul(worldPos, lightView); // world -> view
    shadowPos = mul(shadowPos, lightProj); // view -> clip

    //-1 ~ 1 -> 0 ~ 1
    float2 shadowUV;
    shadowUV.x = (shadowPos.x / shadowPos.w) * 0.5f + 0.5f;
    shadowUV.y = (shadowPos.y / shadowPos.w) * -0.5f + 0.5f;

    float currentDepth = shadowPos.z / shadowPos.w;

    // 범위 밖이면 그림자 없음 처리
    if (shadowPos.w <= 0.f || any(shadowUV < 0.f) || any(shadowUV > 1.f))
        return 0.f;

    uint shadowWidth, shadowHeight;
    shadowTexture.GetDimensions(shadowWidth, shadowHeight);

    float2 shadowTexel = 1.f / float2(max(1u, shadowWidth), max(1u, shadowHeight));

    // 0이면 그림자 없음, 1이면 완전 그림자.
    // 3x3 PCF 결과를 누적해서 부드럽게 만든다.
    float shadowAmount = 0.f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleUV = shadowUV + float2(x, y) * shadowTexel;

            // shadow map에 저장된 depth.
            float storedDepth = shadowTexture.Sample(PointClampSampler, sampleUV).x;
            shadowAmount += (storedDepth + bias < currentDepth) ? 1.f : 0.f;
        }
    }

    // 9번 샘플 중 몇 개가 그림자인지 평균낸다.
    // 0.0 = 그림자 없음, 1.0 = 완전 그림자, 중간값 = 경계가 부드러운 그림자.
    return shadowAmount / 9.f;
}

static const float3 SSAOKernel[16] =
{
    float3(0.5381f, 0.1856f, 0.4319f), float3(0.1379f, 0.2486f, 0.4430f),
    float3(0.3371f, 0.5679f, 0.0057f), float3(-0.6999f, -0.0451f, 0.0019f),
    float3(0.0689f, -0.1598f, 0.8548f), float3(0.0560f, 0.0069f, 0.1843f),
    float3(-0.0146f, 0.1402f, 0.0762f), float3(0.0100f, -0.1924f, 0.0344f),
    float3(-0.3577f, -0.5301f, 0.4358f), float3(-0.3169f, 0.1063f, 0.0158f),
    float3(0.0103f, -0.5869f, 0.0046f), float3(-0.0897f, -0.4940f, 0.3287f),
    float3(0.7119f, -0.0154f, 0.0918f), float3(-0.0533f, 0.0596f, 0.5411f),
    float3(0.0352f, -0.0631f, 0.5460f), float3(-0.4776f, 0.2847f, 0.0271f)
};

float Get_ViewZ(float2 uv)
{
    float2 depth = g_DepthTexture.Sample(PointClampSampler, uv).xy;
    return depth.y * g_DeferredFar; // view space에서의 z 값으로 복구
}

float3 Reconstruct_ViewPos(float2 uv)
{
    float2 depth = g_DepthTexture.Sample(LinearWrapSampler, uv).xy;
    float viewZ = depth.y * g_DeferredFar;
    if (viewZ <= 0.f)
        return 0.f;

    float4 clipPos;
    clipPos.x = uv.x * 2.f - 1.f;
    clipPos.y = uv.y * -2.f + 1.f;
    clipPos.z = depth.x;
    clipPos.w = 1.f;
    clipPos *= viewZ;

    float4 viewPos = mul(clipPos, g_DeferredProjMatrixInverse);
    return viewPos.xyz;
}

float3 Get_NormalWS(float2 uv)
{
    return normalize(DecodeNormal(g_NormalTexture.Sample(PointClampSampler, uv).xyz));
}

float3 Get_NormalVS(float2 uv)
{
    float3 normalWS = Get_NormalWS(uv);
    return normalize(mul(float4(normalWS, 0.f), g_ViewMatrix).xyz);
}

float2 Project_ViewPos_ToUV(float3 viewPos)
{
    float4 clipPos = mul(float4(viewPos, 1.f), g_ProjMatrix);
    float invW = rcp(max(abs(clipPos.w), EPSILON));
    float2 ndc = clipPos.xy * invW;
    return ndc * float2(0.5f, -0.5f) + 0.5f;
}

float3 BuildTangent(float3 normal)
{
    float3 helper = (abs(normal.z) < 0.999f) ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
    return normalize(cross(helper, normal));
}

float Get_SSAO(float2 uv)
{
    return g_SSAOTexture.Sample(LinearClampSampler, uv).r;
}

float Hash12(float2 p)
{
    float h = dot(p, float2(127.1f, 311.7f));
    return frac(sin(h) * 43758.5453123f);
}

// Depth로 AO를 계산한 다음, 이후 Blur 단계에서 부드럽게 함
float Compute_DepthSSAO(float2 uv)
{
    if (g_SSAOEnabled == 0u)
        return 1.f;

    float3 centerPosVS = Reconstruct_ViewPos(uv);
    float centerViewZ = centerPosVS.z;
    float3 centerN = Get_NormalVS(uv);

    if (centerViewZ <= 0.f)
        return 1.f;

    uint sampleCount = min(max(g_SSAOSampleCount, 1u), 16u);
    float occlusion = 0.f;
    float sampleWeightSum = 0.f;

    uint depthWidth, depthHeight;
    g_DepthTexture.GetDimensions(depthWidth, depthHeight);

    float2 pixelCoord = floor(uv * float2(max(depthWidth, 1u), max(depthHeight, 1u)));
    float rand01 = Hash12(pixelCoord);
    float randomAngle = rand01 * 6.28318530718f; // 2PI

    float baseRadius = max(g_SSAORadius, 0.35f);
    float depthBias = max(g_SSAOBias, 1e-4f);
    float depthRange = max(g_SSAODepthRange, baseRadius);

    float3 tangent = BuildTangent(centerN);
    float3 bitangent = normalize(cross(centerN, tangent));
    float s = sin(randomAngle);
    float c = cos(randomAngle);
    float3 rotatedTangent = normalize(tangent * c + bitangent * s);
    float3 rotatedBitangent = normalize(cross(centerN, rotatedTangent));

    [unroll]
    for (int i = 0; i < sampleCount; ++i)
    {
        float sampleT = ((float)i + 0.5f) / (float)sampleCount;
        float sampleScale = lerp(0.1f, 1.f, sampleT * sampleT);
        float3 kernel = normalize(SSAOKernel[i]);
        float3 sampleDirVS =
            rotatedTangent * kernel.x +
            rotatedBitangent * kernel.y +
            centerN * abs(kernel.z);

        float3 samplePosVS = centerPosVS + sampleDirVS * (baseRadius * sampleScale);
        if (samplePosVS.z <= 0.f)
            continue;

        float2 sampleUV = Project_ViewPos_ToUV(samplePosVS);
        if (any(sampleUV < 0.f) || any(sampleUV > 1.f))
            continue;

        float sampleDepthVS = Get_ViewZ(sampleUV);
        if (sampleDepthVS <= 0.f)
            continue;

        float rangeCheck = smoothstep(0.f, 1.f, depthRange / max(abs(centerPosVS.z - sampleDepthVS), EPSILON));
        float occluded = (sampleDepthVS <= samplePosVS.z - depthBias) ? 1.f : 0.f;

        occlusion += occluded * rangeCheck;
        sampleWeightSum += 1.f;
    }

    float visibility = 1.f - (occlusion / max(sampleWeightSum, EPSILON));
    visibility = saturate(visibility);

    return visibility;
}

float Compute_SSAOBilateralBlur(float2 uv)
{
    if (g_SSAOEnabled == 0u)
        return 1.f;

    float centerAO = g_SSAOTexture.Sample(LinearClampSampler, uv).r;
    float centerZ = Get_ViewZ(uv);
    float3 centerN = Get_NormalWS(uv);

    float aoSum = centerAO;
    float weightSum = 1.f;
    float2 texelSize = g_TexelSize;
    int radius = max(1, (int) round(g_SSAOBlurRadius));

    [loop]
    for (int y = -radius; y <= radius; ++y)
    {
        [loop]
        for (int x = -radius; x <= radius; ++x)
        {
            if (x == 0 && y == 0)
                continue;

            float2 sampleUV = uv + float2((float)x, (float)y) * texelSize;
            if (any(sampleUV < 0.f) || any(sampleUV > 1.f))
                continue;

            // blur 할 이웃의 AO, 깊이, 노멀
            float sampleAO = g_SSAOTexture.Sample(LinearClampSampler, sampleUV).r;
            float sampleZ = Get_ViewZ(sampleUV);
            float3 sampleN = Get_NormalWS(sampleUV);

            // joint/cross bilateral blur
            float dist2 = (float)(x * x + y * y);
            float sigmaS = max(g_SSAOBlurRadius, 1.f);
            float wSpatial = exp(-dist2 / (2.f * sigmaS * sigmaS));

            float depthDiff = abs(centerZ - sampleZ);
            float wDepth = exp(-depthDiff * max(g_SSAODepthSharpness, EPSILON));

            float normalDelta = 1.f - saturate(dot(centerN, sampleN));
            float wNormal = exp(-normalDelta * max(g_SSAONormalSharpness, EPSILON));

            float weight = wSpatial * wDepth * wNormal;
            aoSum += sampleAO * weight;
            weightSum += weight;
        }
    }

    return saturate(aoSum / max(weightSum, EPSILON));
}

#endif
