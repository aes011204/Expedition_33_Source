#pragma once
#include <atomic>
#include "Engine_Typedef.h"

namespace Engine::LogRateLimit
{
// callsite별 1회 로그 여부를 원자적으로 기록한다.
struct OnceState
{
    std::atomic_bool logged{ false };
};

// callsite별 제한 로그 카운트를 원자적으로 기록한다.
struct ThrottleState
{
    std::atomic_uint count{ 0u };
};

// 같은 OnceState에서 첫 호출일 때만 true를 반환한다.
inline bool Should_LogOnce(OnceState& state)
{
    return !state.logged.exchange(true);
}

// 첫 호출과 interval 주기 호출에서만 true를 반환한다.
inline bool Should_LogThrottled(ThrottleState& state, uint32 interval)
{
    const uint32 safeInterval = 0u == interval ? 1u : interval;
    const uint32 current = state.count.fetch_add(1u);

    return 0u == current || 0u == current % safeInterval;
}
}
