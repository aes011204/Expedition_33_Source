#pragma once

/*
 * Helper_Math.h
 * 
 * 수학 연산, 보간(Lerp), Easing 함수 등 수학적 헬퍼들을 정의합니다.
 */

#include "Engine_Typedef.h"
#include <algorithm>  // clamp
#include <cmath>      // lerp, exp, pow
#include <random>

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

/**
 * @brief 베를레 적분 (Verlet Integration)
 *
 * 속도 대신 이전 위치 저장: 명시적인 속도 변수(v)를 저장하는 대신,
 * 직전 위치와 현재 위치를 저장하여 움직임을 표현합니다.
 * 물리 법칙 시뮬레이션: 관성, 가속도, 질량 등을 비교적 간단한 연산만으로 처리할 수 있어
 * 래그돌, 로프, 천(Cloth) 등의 물리 표현에 탁월합니다.
 *
 * @param currentPos 현재 위치
 * @param prevPos    직전 프레임의 위치
 * @param force      가해지는 총 힘 (중력 + 관성 + 복원력 등)
 * @param damping    감쇠 계수 (1.0에 가까울수록 감속 없음, 보통 0.5~0.9)
 * @param timeDelta  델타 타임 (프레임 진행 시간)
 * @param pow        적분 가중치 (기본값 2)
 * @return Engine::Vec3 시뮬레이션된 다음 프레임의 위치
 */
inline Engine::Vec3 SolveVerlet(const Engine::Vec3& currentPos, const Engine::Vec3& prevPos, const Engine::Vec3& force, float damping, float timeDelta, int pow = 2)
{
    Engine::Vec3 velocity = (currentPos - prevPos) * damping;
    
    // 최적화: 가장 많이 사용되는 2제곱(dt^2)은 단순 곱셈으로 처리하여 CPU 사이클을 크게 절약합니다.
    float timeWeight = (pow == 2) ? (timeDelta * timeDelta) : std::pow(timeDelta, static_cast<float>(pow));

    return currentPos + velocity + force * timeWeight;
}

inline float GetSmoothStepRatio(float ratio, float power = 2.0f)
{
    ratio = std::clamp(ratio, 0.f, 1.f);
    return ratio * ratio * (3.f - 2.f * ratio);
}

/**
 * @brief value가 start~end 구간 안에서 차지하는 비율을 0~1로 정규화한다.
 */
inline float NormalizeSegment(float value, float start, float end)
{
    if (end <= start)
        return 0.f;

    return std::clamp((value - start) / (end - start), 0.f, 1.f);
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

inline int RandNum(int startNum, int endNum)
{
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<int> disInt(startNum, endNum);

    return disInt(gen);
}

inline unsigned int RandNum(unsigned int startNum, unsigned int endNum)
{
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<unsigned int> disUInt(startNum, endNum);

    return disUInt(gen);
}

inline float RandNum(float startNum, float endNum)
{
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<float> disFloat(startNum, endNum);

    return disFloat(gen);
}

inline float RandNum(float startNum, float endNum, std::mt19937& rng)
{
    std::uniform_real_distribution<float> dist(startNum, endNum);
    return dist(rng);
}

/**
 * @brief 두 방향 벡터 사이의 회전을 나타내는 쿼터니언을 생성합니다. (DirectX SimpleMath 기반)
 * @param v1 정규화된 시작 벡터
 * @param v2 정규화된 목표 벡터
 * @return 두 벡터 사이의 최단 회전을 나타내는 쿼터니언
 */
inline Engine::Quat CreateRotationBetweenVectors(const Engine::Vec3& v1, const Engine::Vec3& v2)
{
    float dot = v1.Dot(v2);
    
    // 두 벡터가 거의 같은 방향일 때
    if (dot >= 1.0f - 1e-6f)
        return Engine::Quat::Identity;
        
    // 두 벡터가 거의 정반대 방향일 때 (180도 회전)
    if (dot <= -1.0f + 1e-6f)
    {
        Engine::Vec3 axis = Engine::Vec3::UnitX.Cross(v1);
        if (axis.LengthSquared() < 1e-6f)
            axis = Engine::Vec3::UnitY.Cross(v1);
        axis.Normalize();
        return Engine::Quat::CreateFromAxisAngle(axis, DirectX::XM_PI);
    }
    
    // 일반적인 경우 (최단 경로 회전)
    Engine::Quat q;
    Engine::Vec3 cross = v1.Cross(v2);
    q.x = cross.x;
    q.y = cross.y;
    q.z = cross.z;
    q.w = 1.0f + dot;
    q.Normalize();
    return q;
}



}
