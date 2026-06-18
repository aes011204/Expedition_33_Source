#include "../Engine_Shader_ConstantBuffer.hlsl"
#include "../Engine_Shader_Samplers.hlsl"

Texture2D<float4> g_FroxelDepthTexture : register(t0);          // 현재 Froxel이 물체보다 뒤에 있는지 판단할 때 사용
Texture2DArray<float> g_FroxelShadowTexture : register(t1);     // cascade shadow map
Texture3D<float4> g_FroxelInjectionTexture : register(t2);      // Infrect Froxel 패스에서 만든 결과를 Accumulate ㅠㅐ스에서 읽을 때 사용

RWTexture3D<float4> g_FroxelVolumeUAV : register(u0);           // Compute Shader가 결과 쓰는 출력 Texture3D

static const float2 ShadowPoissonDisk[4] =
{
    float2(-0.94201624f, -0.39906216f),
    float2(0.94558609f, -0.76890725f),
    float2(-0.09418410f, -0.92938870f),
    float2(0.34495938f, 0.29387760f)
};

// Froxel 인덱스를 실제 월드 위치로 바꾼다
float3 ComputeFroxelWorldPosition(uint3 dispatchThreadID, out float3 froxelPositionVS, out float2 uv, out float viewDepth)
{
    // XY -> uv -> ndc 
    uv = ((float2) dispatchThreadID.xy + 0.5f) / float2(g_FroxelWidth, g_FroxelHeight);
    const float2 ndc = float2(uv.x * 2.f - 1.f, 1.f - uv.y * 2.f);

    // z 슬라이스 -> view depth
    const float sliceRatio = ((float) dispatchThreadID.z + 0.5f) / max((float) g_FroxelDepth, 1.f);
    viewDepth = lerp(g_FroxelNear, g_FroxelFar, sliceRatio);

    // 화면 XY 방향 복원 
    float4 viewPosition = mul(float4(ndc, 1.f, 1.f), g_FroxelInvProjection);
    viewPosition.xyz /= max(viewPosition.w, 0.0001f);

    // view, world space 위치 계산
    const float3 viewRay = viewPosition.xyz / max(viewPosition.z, 0.0001f);
    froxelPositionVS = viewRay * viewDepth;

    return mul(float4(froxelPositionVS, 1.f), g_FroxelInvView).xyz;
}

// 월드 위치를 Shadow Map 좌표로 변환한다.
bool ComputeShadowCoords(
    float4 worldPos,
    row_major float4x4 lightView,
    row_major float4x4 lightProj,
    out float2 shadowUV,
    out float currentDepth)
{
    // -> light clip space
    float4 shadowPos = mul(worldPos, lightView);
    shadowPos = mul(shadowPos, lightProj);

    if (shadowPos.w <= 0.f)
        return false;

    // -> shadow uv
    shadowUV.x = (shadowPos.x / shadowPos.w) * 0.5f + 0.5f;
    shadowUV.y = (shadowPos.y / shadowPos.w) * -0.5f + 0.5f;
    currentDepth = shadowPos.z / shadowPos.w;

    return all(shadowUV >= 0.f) && all(shadowUV <= 1.f) && currentDepth >= 0.f && currentDepth <= 1.f;
}

// 그림자 amount를 4-sampling 하여 계산
float SampleShadowMapPCF(float2 shadowUV, float currentDepth, uint cascadeIndex, float bias)
{
    uint shadowWidth, shadowHeight, shadowLayers;
    g_FroxelShadowTexture.GetDimensions(shadowWidth, shadowHeight, shadowLayers);

    const float2 texelSize = 1.f / float2(max(1u, shadowWidth), max(1u, shadowHeight));

    float shadowAmount = 0.f;

    [unroll]
    for (uint sampleIndex = 0u; sampleIndex < 4u; ++sampleIndex)
    {
        const float2 sampleUV = shadowUV + ShadowPoissonDisk[sampleIndex] * texelSize * 3.f;
        const float storedDepth = g_FroxelShadowTexture.SampleLevel(PointClampSampler, float3(sampleUV, cascadeIndex), 0.f);
        shadowAmount += (storedDepth + bias < currentDepth) ? 1.f : 0.f;
    }

    return shadowAmount / 4.f;
}

// 현재 Froxel의 View Depth를 기준으로 사용할 Cascade를 선택
uint SelectShadowCascade(float viewDepth)
{
    const uint cascadeCount = max(1u, g_FroxelShadowCascadeCount);

    [loop]
    for (uint cascadeIndex = 0u; cascadeIndex < cascadeCount; ++cascadeIndex)
    {
        if (viewDepth <= g_FroxelCascadeSplits[cascadeIndex])
            return cascadeIndex;
    }

    return cascadeCount - 1u;
}

float SampleCascadeShadow(float4 worldPos, float viewDepth)
{
    if (g_FroxelShadowCascadeCount == 0u || g_FroxelShadowIntensity <= 0.f)
        return 0.f;

    const uint cascadeIndex = SelectShadowCascade(viewDepth);

    float2 shadowUV = 0.f;
    float currentDepth = 0.f;
    if (!ComputeShadowCoords(
        worldPos,
        g_FroxelCascadeLightViewMatrices[cascadeIndex],
        g_FroxelCascadeLightProjMatrices[cascadeIndex],
        shadowUV,
        currentDepth))
    {
        return 0.f;
    }

    return SampleShadowMapPCF(shadowUV, currentDepth, cascadeIndex, g_FroxelShadowBias);
}

// 각 Froxel 칸에 대해 이 위치에 빛이 얼마나 존재하는지, 빛이 얼마나 감쇠되는지를 계산해서 저장한다.
[numthreads(8, 8, 1)]
void InjectFroxelBuffer(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= g_FroxelWidth || dispatchThreadID.y >= g_FroxelHeight || dispatchThreadID.z >= g_FroxelDepth)
        return;

    // 현재 Froxel의 실제 위치 계산 
    float3 froxelPositionVS = 0.f;  // view space 위치
    float2 uv = 0.f;                // Froxel의 화면 위치
    float viewDepth = 0.f;          // 카메라 정면 방향으로 얼마나 떨어져 있는지
    const float3 froxelPositionWS   // world space 위치
        = ComputeFroxelWorldPosition(dispatchThreadID, froxelPositionVS, uv, viewDepth);

    // scene depth를 읽어, Froxel이 물체 뒤에 있는지 판단한다.
    const float4 depthSample = g_FroxelDepthTexture.SampleLevel(PointClampSampler, uv, 0.f);
    const float sceneViewDepth = depthSample.y * g_FroxelCameraFar;
    if (sceneViewDepth > 0.f && viewDepth > sceneViewDepth + g_FroxelDepthBias)
    {
        g_FroxelVolumeUAV[dispatchThreadID] = 0.f;
        return;
    }

    float3 accumulatedScattering = 0.f;
    float accumulatedExtinction = 0.f;
    const float3 viewDirectionWS = normalize(g_FroxelCamPosition.xyz - froxelPositionWS);
    const float3 lightTravelDirectionWS = normalize(g_FroxelLightDirection.xyz);
    const float scatteringPhase = saturate(dot(viewDirectionWS, lightTravelDirectionWS) * 0.5f + 0.5f);

    bool shadowResolved = false;
    float shadowVisibility = 1.f;

    // 모든 Light Shaft Zone 검사
    [loop]
    for (uint zoneIndex = 0u; zoneIndex < min(g_LightShaftZoneCount, 8u); ++zoneIndex)
    {
        const VolumetricLightShaftZoneEntry zone = g_LightShaftZones[zoneIndex];
        const float3 localPosition = mul(float4(froxelPositionWS, 1.f), zone.worldToLocal).xyz;
        const float3 distanceToEdge = zone.halfExtents - abs(localPosition);

        const float insideVolume = step(0.f, distanceToEdge.x) * step(0.f, distanceToEdge.y) * step(0.f, distanceToEdge.z);
        if (insideVolume <= 0.f)    // zone 밖
            continue;

        if (!shadowResolved)
        {
            const float shadowAmount = SampleCascadeShadow(float4(froxelPositionWS, 1.f), viewDepth);
            shadowVisibility = lerp(1.f, 1.f - shadowAmount, saturate(g_FroxelShadowStrength));
            shadowResolved = true;
        }

        const float nearestEdgeDistance = min(distanceToEdge.x, min(distanceToEdge.y, distanceToEdge.z));
        const float edgeFactor = smoothstep(0.f, max(zone.edgeFade, 0.0001f), nearestEdgeDistance);
        const float softenedEdgeFactor = edgeFactor * edgeFactor;

        const float zoneDensity = insideVolume * softenedEdgeFactor * zone.density * zone.intensity;
        const float zoneScattering = zoneDensity * scatteringPhase * shadowVisibility;

        // 빛 산란량, 소멸량 누적
        accumulatedScattering += zone.lightColor.rgb * zoneScattering;
        accumulatedExtinction += zoneDensity * 0.08f * shadowVisibility;
    }

    // Injection 결과 저장 
    g_FroxelVolumeUAV[dispatchThreadID] = float4(accumulatedScattering, saturate(accumulatedExtinction));
}

// 카메라 방향으로 빛을 모으는 단계
// InjectFroxelBuffer()에서 각 칸의 개별 정보를 모았다면, AccumulateFroxelBuffer()는 이 칸들을 카메라 방향으로 연결한다.
[numthreads(8, 8, 1)]
void AccumulateFroxelBuffer(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= g_FroxelWidth || dispatchThreadID.y >= g_FroxelHeight)
        return;

    // 누적 빛, 투과율 초기화
    float3 accumulatedLighting = 0.f;
    float transmittance = 1.f;

    // 같은 XY의 모든 슬라이스를 한 번에 누적한다.
    [loop]
    for (uint sliceIndex = 0u; sliceIndex < g_FroxelDepth; ++sliceIndex)
    {
        const float4 froxelSample = g_FroxelInjectionTexture.Load(int4(dispatchThreadID.xy, sliceIndex, 0));
        accumulatedLighting += froxelSample.rgb * transmittance;
        transmittance *= saturate(1.f - froxelSample.a);

        g_FroxelVolumeUAV[uint3(dispatchThreadID.xy, sliceIndex)] = float4(accumulatedLighting, 1.f - transmittance);

        if (transmittance <= 0.001f)
        {
            [loop]
            for (uint remainingSlice = sliceIndex + 1u; remainingSlice < g_FroxelDepth; ++remainingSlice)
                g_FroxelVolumeUAV[uint3(dispatchThreadID.xy, remainingSlice)] = float4(accumulatedLighting, 1.f - transmittance);

            break;
        }
    }
}
