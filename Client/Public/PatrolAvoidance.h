#pragma once
#include "Base.h"

NS_BEGIN(Client)

class Monster_WorldBase;

// Per-patrol state; never moves the owner or changes the patrol route.
class PatrolAvoidance final
{
public:
    enum class State { Direct, Avoiding, Blocked, Unavailable };
    struct Result
    {
        State state = State::Unavailable;
        Vec3 target = Vec3::Zero;
    };

    Result Update(const Shared<Monster_WorldBase>& monster, const Vec3& patrolTarget, float deltaTime);
    void Reset();

private:
    bool _avoiding = false;
    bool _waiting = false;
    int _side = 0;
    Vec3 _waypoint = Vec3::Zero;
    float _clearTime = 0.f;
    float _retryTime = 0.f;
};

NS_END
