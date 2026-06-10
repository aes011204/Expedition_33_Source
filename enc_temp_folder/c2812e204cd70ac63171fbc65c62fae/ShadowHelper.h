#pragma once

#include "ShaderCom.h"
#include "Shader_CBuffer_Define.h"
#include "Shadow.h"
#include "Camera.h"

NS_BEGIN(ShadowHelper)

/*
 Light-space View

                 light dir
                    \
                     \
                      \

        receiver box (sliceCorners만 포함)
              +------------------+
              |                  |
              |      slice       |
              |                  |
              +------------------+

        projection box (실제 shadow map ortho)
          +--------------------------+
          |      xy/z padding        |
          |   +------------------+   |
          |   |   receiver box   |   |
          |   +------------------+   |
          +--------------------------+

        influence volume
        caster가 upstream 쪽 바깥에 있어도
        이 slice 안으로 그림자를 던질 수 있어서 더 늘림

   upstream
      <-------------------------------------- light

          +--------------------------------------+
          |          influence volume            |
          |     +--------------------------+     |
          |     |      projection box      |     |
          |     |   +------------------+   |     |
          |     |   |   receiver box   |   |     |
          |     |   +------------------+   |     |
          |     +--------------------------+     |
          +--------------------------------------+
 */

struct CascadeBounds
{
    // receiver : 현재 cascade sliceCorners를 빛 카메라 공간으로 옮겼을 때의 AABB.
    Vec3 receiverMinLS = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 receiverMaxLS = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    // receiver에서 시작한 빛 카메라 공간에서의 범위에서의 최종 shadow ortho projection. 
    Vec3 minLS = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 maxLS = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    // influence : 현재 cascade가 받아야 할 그림자의 caster까지 포함하려고 넓힌 영역
    Vec3 influenceMinLS = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 influenceMaxLS = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    Vec3 centerWS = Vec3::Zero;             // 현재 cascade slice의 bounding sphere 중심, 월드 공간
    Vec3 eyeWS = Vec3::Zero;                // 그 slice를 보기 위해 잡은 light-view 카메라 위치
    Vec3 centerLS = Vec3::Zero;             // centerWS를 라이트 공간으로 옮긴 값

   
    float stableRadius = 0.f;               // 현재 cascade slice를 감싸는 bounding sphere 반경
    float sliceMaxUpstream = 0.f;           // 현재 slice 코너들 중 upstream 방향으로 가장 멀리 있는 값
    float coverageMaxUpstream = 0.f;        // 전체 coverage 코너들 중 upstream 방향으로 가장 멀리 있는 값
    float coverageRadius = 0.f;             // 전체 shadow coverage 구간을 감싼 sphere 반경
    float centerToCoverageDistance = 0.f;   // 현재 slice 중심에서 전체 coverage 중심까지 거리
    float extrusionDistance = 0.f;          // influence volume을 upstream 방향으로 얼마나 늘릴지 계산된 최종 거리

    // 최종 projection 범위를 shadow map 해상도로 나눈 텍셀 월드 크기
    float texelX = 0.f;
    float texelY = 0.f;
};

inline ShadowCascadePreset& Get_CascadePreset(ShadowCascadePresetSet& presets, CascadeLevel level)
{
    switch (level)
    {
    case CascadeLevel::Close:           return presets.close;
    case CascadeLevel::Far:             return presets.farRange;
    case CascadeLevel::Normal: default: return presets.normal;
    }
}

inline const ShadowCascadePreset& Get_CascadePreset(const ShadowCascadePresetSet& presets, CascadeLevel level)
{
    switch (level)
    {
    case CascadeLevel::Close:           return presets.close;
    case CascadeLevel::Far:             return presets.farRange;
    case CascadeLevel::Normal: default: return presets.normal;
    }
}

inline ShadowCascadePreset Sanitize_CascadePreset(const ShadowCascadePreset& preset)
{
    ShadowCascadePreset sanitized = preset;
    sanitized.focusNear = max(0.001f, sanitized.focusNear);
    sanitized.focusFar = max(sanitized.focusNear + 1.f, sanitized.focusFar);
    sanitized.cascadeSplitLambda = min(1.f, max(0.f, sanitized.cascadeSplitLambda));
    sanitized.cascadeBlendDistance = max(0.f, sanitized.cascadeBlendDistance);
    return sanitized;
}

// 각 cascade의 split 위치를 계산한다. lambda가 0이면 균등 분할, 1이면 로그 분할이 된다.
inline float Compute_CascadeSplit(
    uint32 cascadeIndex,
    uint32 cascadeCount,
    float nearZ,
    float farZ,
    float lambda)
{
    const float ratio = farZ / max(nearZ, 0.0001f);
    const float t = static_cast<float>(cascadeIndex + 1) / static_cast<float>(max(1u, cascadeCount));
    const float logSplit = nearZ * powf(ratio, t);
    const float uniformSplit = nearZ + (farZ - nearZ) * t;
    return uniformSplit + (logSplit - uniformSplit) * lambda;
}

// 8개의 코너를 감싸는 대충의 bounding sphere를 만든다.
inline void Compute_CornersBoundingSphere(
    const Vec3(&corners)[8],
    Vec3& outCenter,
    float& outRadius)
{
    outCenter = Vec3::Zero;
    for (const Vec3& corner : corners)
        outCenter += corner;
    outCenter /= 8.f;

    outRadius = 0.f;
    for (const Vec3& corner : corners)
        outRadius = max(outRadius, (corner - outCenter).Length());
}

// 카메라 slice의 8개의 코너 생성
inline void Build_FrustumCorners(
    const Matrix& cameraWorld,
    float nearZ,
    float farZ,
    float tanHalfFovY,
    float aspect,
    Vec3(&outCorners)[8])
{
    uint32 index = 0;
    const float depthList[2] = { nearZ, farZ };

    for (uint32 depthIndex = 0; depthIndex < 2; ++depthIndex)
    {
        const float z = depthList[depthIndex];
        const float halfY = z * tanHalfFovY;
        const float halfX = halfY * aspect;

        const Vec3 viewCorners[4] =
        {
            Vec3(-halfX,  halfY, z),
            Vec3(halfX,   halfY, z),
            Vec3(halfX,  -halfY, z),
            Vec3(-halfX, -halfY, z),
        };

        for (uint32 cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
            outCorners[index++] = Vec3::Transform(viewCorners[cornerIndex], cameraWorld);
    }
}

// 카메라의 cascade slice 하나를 받아, 이 slice에 대한 shadow view/proj 행렬, caster 영향 범위까지 만든다.
inline void Build_CascadeBounds(
    const Vec3(&sliceCorners)[8],                       /* 카메라의 cascade 구간을 이루는 8개의 월드 좌표 */
    const Vec3(&coverageCorners)[8],
    const Vec3& coverageCenter,
    float coverageRadius,
    const Vec3& lightDir,
    const ShadowLightDesc& desc,
    Matrix& outView,                                    /* 빛 입장에서 어디서 볼지 */
    Matrix& outProj,                                    /* shadow map ortho를 얼마나 크게 잡을지 */
    ShadowCascadeInfluenceVolume& outInfluenceVolume,   /* 어떤 오브젝트까지 이 cascade에 그림자를 드리울 수 있다고 볼지 */
    CascadeBounds& outBounds)
{
    // outInfluenceVolume의 convex hull plane들을 만들 때 중복 평면을 덜 넣기 위해 넣는 보조 함수.
    auto append_hull_plane = [](ShadowCascadeInfluenceVolume& volume, const Vec4& plane)
        {
            constexpr float kNormalDotThreshold = 0.999f;
            constexpr float kDistanceThreshold = 0.05f;

            for (uint32 planeIndex = 0; planeIndex < volume.planeCount; ++planeIndex)
            {
                const Vec4& existing = volume.planes[planeIndex];
                const float normalDot =
                    (existing.x * plane.x) +
                    (existing.y * plane.y) +
                    (existing.z * plane.z);
                if (fabsf(normalDot - 1.f) <= (1.f - kNormalDotThreshold) &&
                    fabsf(existing.w - plane.w) <= kDistanceThreshold)
                {
                    return;
                }
            }

            if (volume.planeCount < ShadowCascadeInfluenceVolume::MaxPlaneCount)
                volume.planes[volume.planeCount++] = plane;
        };

    // 현재 slice의 중심과 반경 계산
    Vec3 center = Vec3::Zero;
    float radius = 0.f;
    Compute_CornersBoundingSphere(sliceCorners, center, radius);

    // light view 행렬 생성하기
    const Vec3 eye = center - lightDir * (radius + desc.lightDistancePadding);
    Vec3 up = Vec3::Up;
    if (fabsf(lightDir.Dot(up)) > 0.95f)
        up = Vec3::Forward;

    outView = XMMatrixLookAtLH(eye, center, up);

    // receiver AABB 생성 : corner들과 center를 light-space 로 옮긴다.
    Vec3 receiverMinLS(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 receiverMaxLS(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const Vec3& corner : sliceCorners)
    {
        const Vec3 lightSpace = Vec3::Transform(corner, outView);
        receiverMinLS = Vec3::Min(receiverMinLS, lightSpace);
        receiverMaxLS = Vec3::Max(receiverMaxLS, lightSpace);
    }

    const Vec3 centerLS = Vec3::Transform(center, outView);
    const float stableRadius = max(1.f, radius);

    // upstream 방향으로 프리스텀 생성하기
    const Vec3 upstreamDir = -lightDir;
    float sliceMaxUpstream = -FLT_MAX;
    float coverageMaxUpstream = -FLT_MAX;

    for (const Vec3& corner : sliceCorners)
        sliceMaxUpstream = max(sliceMaxUpstream, corner.Dot(upstreamDir));          // 현재 cascade
    for (const Vec3& corner : coverageCorners)  
        coverageMaxUpstream = max(coverageMaxUpstream, corner.Dot(upstreamDir));    // 전체 cascade

    // projection 범위 구하기 :  순수 수신 범위인 receiver에서 시작한다.
    Vec3 projectionMinLS = receiverMinLS;
    Vec3 projectionMaxLS = receiverMaxLS;

    // XY footprint는 frustum corner AABB보다 bounding sphere 반경을 기준으로 잡아야
    // 카메라 회전 시 cascade ortho 범위가 과하게 재편성되지 않는다.
    projectionMinLS.x = centerLS.x - stableRadius - desc.xyPadding;
    projectionMaxLS.x = centerLS.x + stableRadius + desc.xyPadding;
    projectionMinLS.y = centerLS.y - stableRadius - desc.xyPadding;
    projectionMaxLS.y = centerLS.y + stableRadius + desc.xyPadding;
    projectionMinLS.z -= desc.zPadding;
    projectionMaxLS.z += desc.zPadding;

    // coverage와 현재 slice를 이용하여, upstream 방향으로 충분히 긴 확장 거리
    const float centerToCoverageDistance = (coverageCenter - center).Length();
    const float extrusionDistance = centerToCoverageDistance + coverageRadius + stableRadius;// +desc.zPadding;

    // influence 범위를 projection에서 시작해서 upstream으로 확장
    Vec3 influenceMinLS = projectionMinLS;
    Vec3 influenceMaxLS = projectionMaxLS;
    influenceMinLS.z = min(influenceMinLS.z, receiverMinLS.z - extrusionDistance);
    projectionMinLS.z = influenceMinLS.z;

    // 현재 캐스케이드 공간을 빛 카메라 공간에서 shadow texel 격자에 스냅시키는 단계
    const float extentX = max(1.f, projectionMaxLS.x - projectionMinLS.x);
    const float extentY = max(1.f, projectionMaxLS.y - projectionMinLS.y);
    const float texelX = extentX / static_cast<float>(max(1u, desc.shadowMapWidth));
    const float texelY = extentY / static_cast<float>(max(1u, desc.shadowMapHeight));

    projectionMinLS.x = floorf(projectionMinLS.x / texelX) * texelX;
    projectionMaxLS.x = ceilf(projectionMaxLS.x / texelX) * texelX;
    projectionMinLS.y = floorf(projectionMinLS.y / texelY) * texelY;
    projectionMaxLS.y = ceilf(projectionMaxLS.y / texelY) * texelY;

    if (projectionMaxLS.z <= projectionMinLS.z)
        projectionMaxLS.z = projectionMinLS.z + 1.f;

    if (influenceMaxLS.z <= influenceMinLS.z)
        influenceMaxLS.z = influenceMinLS.z + 1.f;

    // 최종 shadow projection 생성
    outProj = XMMatrixOrthographicOffCenterLH(
        projectionMinLS.x, projectionMaxLS.x,
        projectionMinLS.y, projectionMaxLS.y,
        projectionMinLS.z, projectionMaxLS.z);

    // 월드 공간에서 현재 캐스케이드 slice 프러스텀을 빛이 오는 방향 쪽으로 늘린 프리즘
    Vec3 influencePoints[16]{};
    for (uint32 cornerIndex = 0; cornerIndex < 8u; ++cornerIndex)
    {
        influencePoints[cornerIndex] = sliceCorners[cornerIndex];
        influencePoints[cornerIndex + 8u] = sliceCorners[cornerIndex] + upstreamDir * extrusionDistance;
    }

    Vec3 influenceMinWS(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 influenceMaxWS(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const Vec3& point : influencePoints)
    {
        influenceMinWS = Vec3::Min(influenceMinWS, point);
        influenceMaxWS = Vec3::Max(influenceMaxWS, point);
    }

    outInfluenceVolume = {};
    outInfluenceVolume.bounds.Center = (influenceMinWS + influenceMaxWS) * 0.5f;
    outInfluenceVolume.bounds.Extents = (influenceMaxWS - influenceMinWS) * 0.5f;

    Vec3 insidePoint = Vec3::Zero;
    for (const Vec3& point : influencePoints)
        insidePoint += point;
    insidePoint /= 16.f;

    // 세 점 조합으로 평면 후보 생성. 월드 공간에서 convex hull의 면을 찾는 과정
    constexpr float kPlanePointEpsilon = 0.01f;
    for (uint32 i = 0; i < 16u; ++i)
    {
        for (uint32 j = i + 1u; j < 16u; ++j)
        {
            for (uint32 k = j + 1u; k < 16u; ++k)
            {
                Vec3 normal = (influencePoints[j] - influencePoints[i]).Cross(influencePoints[k] - influencePoints[i]);
                if (normal.LengthSquared() < 0.0001f)
                    continue;

                normal.Normalize();
                float d = -normal.Dot(influencePoints[i]);

                uint32 positiveCount = 0u;
                uint32 negativeCount = 0u;
                for (const Vec3& point : influencePoints)
                {
                    const float distance = normal.Dot(point) + d;
                    if (distance > kPlanePointEpsilon)
                        ++positiveCount;
                    else if (distance < -kPlanePointEpsilon)
                        ++negativeCount;
                }

                if (positiveCount > 0u && negativeCount > 0u)
                    continue;

                if (positiveCount > 0u)
                {
                    normal *= -1.f;
                    d *= -1.f;
                }

                if ((normal.Dot(insidePoint) + d) > 0.f)
                {
                    normal *= -1.f;
                    d *= -1.f;
                }

                append_hull_plane(outInfluenceVolume, Vec4(normal.x, normal.y, normal.z, d));
            }
        }
    }

    outBounds.minLS = projectionMinLS;
    outBounds.maxLS = projectionMaxLS;
    outBounds.receiverMinLS = receiverMinLS;
    outBounds.receiverMaxLS = receiverMaxLS;
    outBounds.influenceMinLS = influenceMinLS;
    outBounds.influenceMaxLS = influenceMaxLS;
    outBounds.centerWS = center;
    outBounds.eyeWS = eye;
    outBounds.centerLS = centerLS;
    outBounds.stableRadius = stableRadius;
    outBounds.sliceMaxUpstream = sliceMaxUpstream;
    outBounds.coverageMaxUpstream = coverageMaxUpstream;
    outBounds.coverageRadius = coverageRadius;
    outBounds.centerToCoverageDistance = centerToCoverageDistance;
    outBounds.extrusionDistance = extrusionDistance;
    outBounds.texelX = texelX;
    outBounds.texelY = texelY;
}

NS_END
