#ifndef ENGINE_SHADER_DEFERRED_HLSL
#define ENGINE_SHADER_DEFERRED_HLSL

#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Utils.hlsl"

#define MIN_SSAO_RADIUS 0.05f           // View Space에서 샘플링 반경의 최소값
#define MIN_SSAO_DEPTH_BIAS  0.0001f    // 같은 표면을 occluder로 오판하지 않도록 최소 depth bias를 보장

//==================================================
// TEXTURE
//==================================================
Texture2D g_Texture;
Texture2D g_DiffuseTexture;
Texture2D g_NormalTexture;
Texture2D g_ShadeTexture;
Texture2D g_AmbientTexture;
Texture2D g_SpecularTexture;
Texture2D g_DepthTexture;
Texture2D g_LightDepthTexture;
Texture2D g_MaterialTexture;
Texture2D g_EmissiveTexture;
Texture2D g_StaticLightDepthTexture;
Texture2D g_SSAOTexture;
Texture2D g_SSAOBlurTexture;
TextureCube g_IrradianceMap;
TextureCube g_PrefilterMap;
Texture2D g_BRDFLUT;

//==================================================
// LIGHT
//==================================================


float4 Reconstruct_WorldPos(float2 Texcoord)
{
    vector DepthDesc = g_DepthTexture.Sample(PointClampSampler, Texcoord);
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

// hemisphere sample kernel
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
    return depth.y * g_DeferredFar; // view space에서의 z값
}

float3 Reconstruct_ViewPos(float2 uv)
{
    float2 depth = g_DepthTexture.Sample(PointClampSampler, uv).xy;
    float viewZ = depth.y * g_DeferredFar; // view space에서의 z 값
    if (viewZ <= 0.f)
        return 0.f;

    // ndc -> clip space
    float4 clipPos;
    clipPos.x = uv.x * 2.f - 1.f;
    clipPos.y = uv.y * -2.f + 1.f;
    clipPos.z = depth.x;
    clipPos.w = 1.f;
    clipPos *= viewZ;

    // clip space -> view space
    float4 viewPos = mul(clipPos, g_DeferredProjMatrixInverse);
    return viewPos.xyz;
}

float3 Get_NormalWS(float2 uv)
{
    // 월드의 normal
    return normalize(DecodeNormal(g_NormalTexture.Sample(PointClampSampler, uv).xyz));
}

float3 Get_NormalVS(float2 uv)
{
    // view의 normal
    float3 normalWS = Get_NormalWS(uv);
    return normalize(mul(float4(normalWS, 0.f), g_ViewMatrix).xyz);
}

float2 Project_ViewPos_ToUV(float3 viewPos)
{
    // view space -> clip space
    float4 clipPos = mul(float4(viewPos, 1.f), g_ProjMatrix);
    float invW = rcp(max(abs(clipPos.w), EPSILON));

    // clip sapce -> ndc space
    float2 ndc = clipPos.xy * invW;

    // (-1 ~ 1) -> uv(0 ~ 1)
    return ndc * float2(0.5f, -0.5f) + 0.5f;
}

float3 Build_Tangent(float3 normal)
{
    // normal에 수직인 tangent 를 임시로 만들어줌
    float3 helper = (abs(normal.z) < 0.999f) ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
    return normalize(cross(helper, normal));
}

float Get_SSAO(float2 uv)
{
    return g_SSAOTexture.Sample(LinearClampSampler, uv).r;
}

float Hash12(float2 pixel)
{
    // 픽셀 좌표를 넣으면 0 ~ 1 사이의 랜덤 값
    float h = dot(pixel, float2(127.1f, 311.7f));
    return frac(sin(h) * 43758.5453123f);
}

float2 Get_SSAOPixelCoord(float2 uv)
{
    uint width, height;
    g_DepthTexture.GetDimensions(width, height);
    return floor(uv * float2(max(width, 1u), max(height, 1u)));
}

// Depth로 AO를 계산한 다음, 이후 Blur 단계에서 부드럽게 함
float Compute_DepthSSAO(float2 uv)
{
    if (g_SSAOEnabled == 0u)
        return 1.f;

    // 해당 픽셀의 view space 위치, z, normal
    float3 centerPosVS = Reconstruct_ViewPos(uv);
    float centerViewZ = centerPosVS.z;
    float3 centerN = Get_NormalVS(uv);

    if (centerViewZ <= 0.f)
        return 1.f;

    uint sampleCount = min(max(g_SSAOSampleCount, 1u), 16u);
    float occlusion = 0.f;
    float sampleWeightSum = 0.f;

    // 각 픽셀마다 다른 SSAO 커널 회전 각도로 샘플링하여, 패턴 생성 방지
    float2 pixelCoord = Get_SSAOPixelCoord(uv);
    float rand01 = Hash12(pixelCoord);
    float randomAngle = rand01 * 6.28318530718f; // 2PI
    float radiusJitter = lerp(0.9f, 1.1f, Hash12(pixelCoord + 19.19f));

    float baseRadius = max(g_SSAORadius, MIN_SSAO_RADIUS);
    float baseDepthBias = max(g_SSAOBias, MIN_SSAO_DEPTH_BIAS);

    // 해당 픽셀과 샘플 픽셀의 View Z 차이가 너무 커, SSAO 계산에서 제외해야 할 때 쓰인다. 
    float depthRange = max(g_SSAODepthRange, baseRadius);

    // 샘플링용 좌표축에 랜덤 회전 적용
    float3 tangent = Build_Tangent(centerN);
    float3 bitangent = normalize(cross(centerN, tangent));
    float s = sin(randomAngle);
    float c = cos(randomAngle);
    float3 rotatedTangent = normalize(tangent * c + bitangent * s);
    float3 rotatedBitangent = normalize(cross(centerN, rotatedTangent));

    [unroll]
    for (int i = 0; i < sampleCount; ++i)
    {
        // 샘플을 중심 픽셀 근처에 몰아주기 위함
        float sampleT = ((float) i + 0.5f) / (float) sampleCount;
        float sampleScale = lerp(0.1f, 1.f, sampleT * sampleT);
        float3 kernel = normalize(SSAOKernel[i]);

        // SSAOKernal의 샘플 방향을 해당 픽셀의 랜덤 회전된 축 기준으로 변환
        float3 sampleDirVS =
            rotatedTangent * kernel.x +
            rotatedBitangent * kernel.y +
            centerN * abs(kernel.z);

        // 샘플 위치(VS)를 화면에 투영한 뒤, 그 화면 위치에 실제로 찍힌 Depth와 비교한다
        float sampleRadius = baseRadius * sampleScale * radiusJitter;
        float sampleDepthBias = max(baseDepthBias * sampleScale, MIN_SSAO_DEPTH_BIAS);
        float sampleDepthRange = max(depthRange * sampleScale, sampleDepthBias * 2.f);
        float3 samplePosVS = centerPosVS + sampleDirVS * sampleRadius;
        if (samplePosVS.z <= 0.f)
            continue;

        float2 sampleUV = Project_ViewPos_ToUV(samplePosVS);
        if (any(sampleUV < 0.f) || any(sampleUV > 1.f))
            continue;

        // samplePosVS를 화면에 투영한 sampleUV에서 Depth Texture를 읽어온 실제 화면에 찍힌 지오메트리 깊이
        float sampleDepthVS = Get_ViewZ(sampleUV);
        if (sampleDepthVS <= 0.f)
            continue;

        float absDepthDiff = abs(centerPosVS.z - sampleDepthVS);
        if (absDepthDiff > sampleDepthRange)
            continue;

        // normal-based weighting
        float3 sampleNormalVS = Get_NormalVS(sampleUV);
        float normalAlignment = saturate(dot(centerN, sampleNormalVS));
        float normalWeight = 1.f; //smoothstep(0.2f, 0.9f, normalAlignment);

        float rangeCheck = 1.f - smoothstep(0.f, sampleDepthRange, absDepthDiff);
        float sampleWeight = rangeCheck * normalWeight;

        // 실제 Depth에 기록된 z가 가상 샘플 위치의 z보다 앞에 있으면 occluded로 판단한다.
        float occluded = (sampleDepthVS <= samplePosVS.z - sampleDepthBias) ? 1.f : 0.f;

        occlusion += occluded * sampleWeight;
        sampleWeightSum += sampleWeight;
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
    float2 pixelCoord = Get_SSAOPixelCoord(uv);
    int radius = max(1, (int) round(g_SSAOBlurRadius));
    float blurAngle = Hash12(pixelCoord + 53.17f) * 6.28318530718f;
    float blurSin = sin(blurAngle);
    float blurCos = cos(blurAngle);
    float2 blurJitter = float2(
        Hash12(pixelCoord + 11.3f),
        Hash12(pixelCoord + 29.7f)) - 0.5f;
    float2 blurJitterUV = blurJitter * texelSize * 0.35f;
    float sigmaS = max(g_SSAOBlurRadius * 0.5f, 1.f);
    float aoRange = 0.02f + min((float) radius, 16.f) * 0.005f;

    [loop]
    for (int y = -radius; y <= radius; ++y)
    {
        [loop]
        for (int x = -radius; x <= radius; ++x)
        {
            if (x == 0 && y == 0)
                continue;

            float2 kernelOffset = float2((float) x, (float) y);
            float2 rotatedOffset = float2(
                kernelOffset.x * blurCos - kernelOffset.y * blurSin,
                kernelOffset.x * blurSin + kernelOffset.y * blurCos);
            float2 sampleUV = uv + rotatedOffset * texelSize + blurJitterUV;
            if (any(sampleUV < 0.f) || any(sampleUV > 1.f))
                continue;

            // blur 할 이웃의 AO, 깊이, 노멀
            float sampleAO = g_SSAOTexture.Sample(LinearClampSampler, sampleUV).r;
            float sampleZ = Get_ViewZ(sampleUV);
            float3 sampleN = Get_NormalWS(sampleUV);

            // joint/cross bilateral blur
            float dist2 = dot(rotatedOffset, rotatedOffset);
            float wSpatial = exp(-dist2 / (2.f * sigmaS * sigmaS));

            float depthDiff = abs(centerZ - sampleZ);
            float wDepth = exp(-depthDiff * max(g_SSAODepthSharpness, EPSILON));

            float normalDelta = 1.f - saturate(dot(centerN, sampleN));
            float wNormal = exp(-normalDelta * max(g_SSAONormalSharpness, EPSILON));

            float aoDiff = abs(centerAO - sampleAO);
            float aoSimilarity = 1.f - smoothstep(0.f, aoRange, aoDiff);
            float wAO = lerp(1.f, aoSimilarity, 0.35f);

            float weight = wSpatial * wDepth * wNormal * wAO;
            aoSum += sampleAO * weight;
            weightSum += weight;
        }
    }

    return saturate(aoSum / max(weightSum, EPSILON));
}


static const float PI = 3.14159265359f;

// 프레넬 반사(근사). 정면에서 볼 때 반사가 약하고, 비스듬히 보면 반사가 세지도록 한다. 
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    // F0 : 정면에서 봤을 때 기본 반사율
    // cosTheta : 얼마나 정면인지
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// GGX Specular의 하이라이트 모양을 정하는 공식. GGX NDF
float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness * roughness * roughness;
    float denom = max((NdotH * NdotH) * (a - 1.0f) + 1.0f, EPSILON);
    return a / (PI * denom * denom);
}

// GGX의 Geometry 항을 Schlick 방식으로 근사 계산. 미세 표면에 의한 가림 보정
float GeometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) * 0.125f;
    return NdotX / max(NdotX * (1.0f - k) + k, EPSILON);
}

// Smith Geometry Term 계열 
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggxView = GeometrySchlickGGX(NdotV, roughness); // 카메라 방향으로 나오는 반사광이 얼마나 가려지는지
    float ggxLight = GeometrySchlickGGX(NdotL, roughness); // 빛이 표면으로 들어올 때 얼마나 가려지는지
    return ggxView * ggxLight;
}

float2 EnvBRDFApprox(float roughness, float NoV)
{
    const float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
    const float4 c1 = float4(1.0f, 0.0425f, 1.04f, -0.04f);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28f * NoV)) * r.x + r.y;
    return float2(-1.04f, 1.04f) * a004 + r.zw;
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0) *
        pow(saturate(1.0f - cosTheta), 5.0f);
}


#endif
