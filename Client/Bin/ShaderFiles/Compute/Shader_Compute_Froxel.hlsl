#include "../Engine_Shader_ConstantBuffer.hlsl"

RWTexture3D<float4> g_FroxelVolumeUAV : register(u0);
Texture3D<float4> g_FroxelTexture : register(t0);


// 카메라 시야 공간을 3D 격자로 나눠 놓고, 현재는 Z 방향으로만 그라데이션
[numthreads(8, 8, 1)]
void InJectFroxelBuffer(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // 현재 담당 중인 Froxel 칸
    if (dispatchThreadID.x >= g_FroxelWidth || dispatchThreadID.y >= g_FroxelHeight || dispatchThreadID.z >= g_FroxelDepth)
        return;

    uint2 froxelXY = dispatchThreadID.xy;

    // 각 칸의 좌상단 모서리가 아닌 각 칸의 중앙 위치를 위해 0.5f 를 더해준다.
    float2 uv = ((float2) dispatchThreadID.xy + 0.5f) / float2(g_FroxelWidth, g_FroxelHeight);

    // uv -> ndc
    float2 ndc = float2(uv.x * 2.f - 1.f, 1.f - uv.y * 2.f);

    // z slice 중심의 선형 분할
    float sliceRatio = ((float) dispatchThreadID.z + 0.5f) / (float) g_FroxelDepth;
    float viewDepth = lerp(g_FroxelNear, g_FroxelFar, sliceRatio);

    // 현재 Froxel의 화면 XY 방향을 구하기 위해, NDC far plane(z = 1)의 한 점을 View Space로 역투영한다.
    float4 viewPosition = mul(float4(ndc, 1.f, 1.f), g_FroxelInvProjection);
    viewPosition.xyz /= max(viewPosition.w, 0.0001f);

    // View Space Z가 1이 되도록 만들어, 이후 원하는 viewDepth를 곱할 수 있는 광선 형태로 변환한다.
    float3 viewRay = viewPosition.xyz / max(viewPosition.z, 0.0001f);

    float3 froxelPositionVS = viewRay * viewDepth;
    float3 froxelPositionWS = mul(float4(froxelPositionVS, 1.f), g_FroxelInvView).xyz;

    float3 accumulatedScattering = 0.f;

    float r = 0.f;
    [loop]
    for (uint zoneIndex = 0u; zoneIndex < min(g_LightShaftZoneCount, 8u); ++zoneIndex)
    {
        VolumetricLightShaftZoneEntry zone = g_LightShaftZones[zoneIndex];

        // froxel 을 빛기둥 로컬 스페이스로 변환 
        float3 localPosition = mul(float4(froxelPositionWS, 1.f), zone.worldToLocal).xyz;

        // 로컬 박스 중심으로부터 각 축의 거리
        float3 distanceToEdge = zone.halfExtents - abs(localPosition);

        // 한 축이라도 음수면 박스 바깥
        float insideVolume = step(0.f, distanceToEdge.x) * step(0.f, distanceToEdge.y) * step(0.f, distanceToEdge.z);
        if (insideVolume <= 0.f)
            continue;

        // 박스 경계에 가까울수록 부드럽게 약해짐
        float nearestEdgeDistance = min(distanceToEdge.x, min(distanceToEdge.y, distanceToEdge.z));

        float edgeFactor = smoothstep(0.f, max(zone.edgeFade, 0.0001f), nearestEdgeDistance);
        float3 viewDirectionWS = normalize(g_FroxelCamPosition.xyz - froxelPositionWS);
        float3 lightTravelDirectionWS = normalize(g_FroxelLightDirection.xyz);

        float scatteringAmount = dot(viewDirectionWS, lightTravelDirectionWS) * 0.5f + 0.5f;
        scatteringAmount = saturate(scatteringAmount);

        float zoneScattering = insideVolume * edgeFactor * zone.density * zone.intensity * scatteringAmount;

        accumulatedScattering += zone.lightColor.rgb * zoneScattering;
        r = 1.f;

    }

    g_FroxelVolumeUAV[dispatchThreadID] = float4(accumulatedScattering, 1.f);
}
