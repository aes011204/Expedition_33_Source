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

static const float2 SSAODirections[8] = {
    float2(1.f, 0.f), float2(-1.f, 0.f), float2(0.f, 1.f), float2(0.f, -1.f),
    float2(1.f, 1.f), float2(-1.f, 1.f), float2(1.f, -1.f), float2(-1.f, -1.f)
};

float Get_ViewZ(float2 uv)
{
    float2 depth = g_DepthTexture.Sample(PointClampSampler, uv).xy;
    return depth.y * g_DeferredFar;
}

float Compute_DepthSSAO(float2 uv)
{
    // 프레임 때문에 16 샘플로 가는데, 많이 어색하면 5x5 샘플로 갈거임. 이건 1.5베 더 비싸짐.
    float centerViewZ = Get_ViewZ(uv);

    if (centerViewZ <= 0.f)
        return 1.f;

    float occlusion = 0.f;

    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        float2 sampleUV = uv + SSAODirections[i] * g_SSAORadius;
        float sampleViewZ = Get_ViewZ(sampleUV);

        if (sampleViewZ <= 0.f)
            continue;

        float depthDiff = centerViewZ - sampleViewZ;

        float isOccluder = depthDiff > g_SSAOBias ? 1.f : 0.f;
        float rangeWeight = saturate(1.f - abs(depthDiff) / g_SSAORadius);

    }

}

#endif
