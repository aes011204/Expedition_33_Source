#pragma once

#include "Client_Defines.h"

NS_BEGIN(Client)

// SkillTree preview 외곽광 전용 feature 계약.
inline constexpr uint32 DrawFeatureFlagPreviewRim = (1u << 2);

struct PreviewRimRuntime
{
    bool enabled = false;
    Vec3 color = Vec3(1.0f, 0.88f, 0.52f);  // 노란색
    float intensity = 1.75f;                // 외곽광 밝기
    float power = 1.8f;                     // 값이 클수록 가장자리 쪽에만 붙는다.
    float hairIntensity = 0.9f;

    void Start(const Vec3& rimColor, float rimIntensity, float rimPower, float rimHairIntensity)
    {
        enabled = true;
        color = rimColor;
        intensity = rimIntensity;
        power = rimPower;
        hairIntensity = rimHairIntensity;
    }

    void Stop()
    {
        enabled = false;
    }
};


// UI Preview에서 공통으로 쓰는 조명과 림 라이트 기본값들
constexpr Vec4 kPreviewLightAmbient = Vec4(0.02f, 0.02f, 0.025f, 1.f);
constexpr Vec4 kPreviewLightSpecular = Vec4(1.45f, 1.30f, 1.08f, 1.f);
constexpr Vec3 kPreviewLightDirection = Vec3(1.572f, 0.319f, -1.061f);
constexpr Vec4 kPreviewLightDiffuse = Vec4(5.f, 4.f, 4.f, 1.f);
constexpr Vec3 kPreviewRimColor = Vec3(1.0f, 0.90f, 0.62f);
constexpr float kPreviewRimIntensity = 1.2f;
constexpr float kPreviewRimPower = 2.12f;
constexpr float kPreviewHairRimIntensity = 0.55f;
constexpr float kPreviewShadowIntensity = 1.15f;

NS_END
