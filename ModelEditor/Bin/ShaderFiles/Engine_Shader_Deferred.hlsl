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

static const float2 SSAODirections[16] =
{
    float2(1.0000f, 0.0000f), float2(-1.0000f, 0.0000f), float2(0.0000f, 1.0000f), float2(0.0000f, -1.0000f),
    float2(0.7071f, 0.7071f), float2(-0.7071f, 0.7071f), float2(0.7071f, -0.7071f), float2(-0.7071f, -0.7071f),
    float2(0.9239f, 0.3827f), float2(-0.9239f, 0.3827f), float2(0.9239f, -0.3827f), float2(-0.9239f, -0.3827f),
    float2(0.3827f, 0.9239f), float2(-0.3827f, 0.9239f), float2(0.3827f, -0.9239f), float2(-0.3827f, -0.9239f)
};

float Get_ViewZ(float2 uv)
{
    float2 depth = g_DepthTexture.Sample(PointClampSampler, uv).xy;
    return depth.y * g_DeferredFar; // view space에서의 z 값으로 복구
}

float3 Get_NormalWS(float2 uv)
{
    return normalize(DecodeNormal(g_NormalTexture.Sample(PointClampSampler, uv).xyz));
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

float2 Rotate2D(float2 v, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return float2(c * v.x - s * v.y, s * v.x + c * v.y);
}

// Depth로 AO를 계산한 다음, 이후 Blur 단계에서 부드럽게 함
float Compute_DepthSSAO(float2 uv)
{
    if (g_SSAOEnabled == 0u)
        return 1.f;

    // 프레임 때문에 16 샘플로 가는데, 많이 어색하면 5x5 샘플로 갈거임. 이러면 1.5베 더 비싸짐.
    float centerViewZ = Get_ViewZ(uv);
    float3 centerN = Get_NormalWS(uv);

    if (centerViewZ <= 0.f)
        return 1.f;

    uint sampleCount = min(max(g_SSAOSampleCount, 1u), 16u);
    float occlusion = 0.f;
    float totalWeight = 0.f;

    float rand01 = Hash12(uv * float2(1600.0f, 900.0f));
    float randomAngle = rand01 * 6.28318530718f; // 2PI

    [unroll]
    for (int i = 0; i < sampleCount; ++i)
    {
        float2 dir = Rotate2D(SSAODirections[i], randomAngle);

        float radiusScale = saturate(10.f / max(centerViewZ, EPSILON));
        float sampleRadius = max(g_SSAORadius * radiusScale, 1.f);

        float2 sampleUV = uv + dir * g_TexelSize * sampleRadius;
        if (any(sampleUV < 0.f) || any(sampleUV > 1.f))     // 화면 밖
            continue;

        // 주변 픽셀의 깊이, 노멀
        float sampleViewZ = Get_ViewZ(sampleUV);
        float3 sampleN = Get_NormalWS(sampleUV);

        if (sampleViewZ <= 0.f) 
            continue;

        float depthDiff = centerViewZ - sampleViewZ;
        if (depthDiff <= 0.f)
            continue;

        float absDepthDiff = depthDiff;

        float occluderSoftness = max(g_SSAOBias * 2.0f, 1e-4f);
        float isOccluder = smoothstep(g_SSAOBias, g_SSAOBias + occluderSoftness, absDepthDiff);

        if (absDepthDiff > g_SSAODepthRange)
            continue;

        float rangeWeight = saturate(1.f - absDepthDiff / max(g_SSAODepthRange, EPSILON));

        // 노말이 비슷할수록 같은 표면일 가능성이 높다.
        float nd = saturate(dot(centerN, sampleN));
        float normalWeight = smoothstep(0.2f, 0.9f, nd);

        float weight = rangeWeight * normalWeight;
        occlusion += isOccluder * weight;
        totalWeight += weight;
    }

    // 1이면 안 어둡고, 0이면 어두워짐
    float visibility = 1.f - (occlusion / max(totalWeight, EPSILON));
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
