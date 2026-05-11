#pragma once

/*
 * Helper_Math.h
 * 
 * 수학 연산, 보간(Lerp), Easing 함수 등 수학적 헬퍼들을 정의합니다.
 */

#include "Engine_Typedef.h"
#include <algorithm>  // clamp
#include <cmath>      // lerp, exp, pow

namespace Engine::Math
{
/**
 * @brief 부드러운 보간을 위한 감쇠 비율 계산
 * 
 * @param speed     감쇠 속도 (값이 클수록 빨리 도착)
 * @param timeDelta 프레임 시간
 * @return 0.0 ~ 1.0 사이의 비율
 */
inline float GetDampingRatio(float speed, float timeDelta)
{
    speed = std::max(0.0f, speed);
    timeDelta = std::max(0.0f, timeDelta);

    // 1 - e^(-speed * dt)
    const float ratio = 1.0f - std::exp(-speed * timeDelta);
    return std::clamp(ratio, 0.0f, 1.0f);
}

/**
 * @brief 가속 보간 비율 계산 (안전 보정 포함)
 *
 * @param ratio 진행 비율 (0.f ~ 1.f)
 * @param power 가속 강도. 제곱 연산. 기본값 2.f
 * @return 0.0 ~ 1.0 사이의 비율
 */
inline float GetEaseInRatio(float ratio, float power = 2.0f)
{
    const float clampedRatio = std::clamp(ratio, 0.0f, 1.0f);
    const float clampedPower = std::max(0.0f, power);

    // 비율을 거듭제곱해 가속형 곡선을 만듦
    const float easeRatio = std::pow(clampedRatio, clampedPower);
    return std::clamp(easeRatio, 0.0f, 1.0f);
}

inline float GetSmoothStepRatio(float ratio, float power = 2.0f)
{
    ratio = std::clamp(ratio, 0.f, 1.f);
    return ratio * ratio * (3.f - 2.f * ratio);
}

/**
 * @brief 감속 보간
 * 
 * 목표 지점에 가까워질수록 속도가 줄어들며 부드럽게 정지한다.
 * 지원 타입: float, Vec2, Vec3
 * 제외 타입: _vec4, _quat
 * 
 * @param start     현재 위치
 * @param end       목표 위치
 * @param speed     감쇠 속도
 * @param timeDelta 프레임 시간
 */
inline float Lerp_Damp(float start, float end, float speed, float timeDelta)
{
    const float ratio = GetDampingRatio(speed, timeDelta);
    return std::lerp(start, end, ratio);
}

inline Vec2 Lerp_Damp(const Vec2& start, const Vec2& end, float speed, float timeDelta)
{
    const float ratio = GetDampingRatio(speed, timeDelta);
    return start + (end - start) * ratio;
}

inline Vec3 Lerp_Damp(const Vec3& start, const Vec3& end, float speed, float timeDelta)
{
    const float ratio = GetDampingRatio(speed, timeDelta);
    return start + (end - start) * ratio;
}

/**
 * @brief 가속 보간
 *
 * 지원 타입: float, Vec2, Vec3
 * 현재 제외 타입: _vec4, _quat
 * 
 * @param initStart 초기 시작 위치
 * @param end       목표 위치
 * @param ratio     진행 비율 (0.f ~ 1.f)
 * @param power     가속 강도. 제곱 연산. 기본값 2.f
 */
inline float Lerp_EaseIn(float initStart, float end, float ratio, float power = 2.0f)
{
    const float easeRatio = GetEaseInRatio(ratio, power);
    return std::lerp(initStart, end, easeRatio);
}

inline Vec2 Lerp_EaseIn(const Vec2& initStart, const Vec2& end, float ratio, float power = 2.0f)
{
    const float easeRatio = GetEaseInRatio(ratio, power);
    return initStart + (end - initStart) * easeRatio;
}

inline Vec3 Lerp_EaseIn(const Vec3& initStart, const Vec3& end, float ratio, float power = 2.0f)
{
    const float easeRatio = GetEaseInRatio(ratio, power);
    return initStart + (end - initStart) * easeRatio;
}
}
