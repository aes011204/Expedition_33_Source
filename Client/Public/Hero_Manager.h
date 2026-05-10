#pragma once

#include "Base.h"

NS_BEGIN(Client)

class Player;
class TargetCamera;
class Body_Player;

class Hero_Manager : public Base
{
public:
    Hero_Manager() = default;
    virtual ~Hero_Manager() = default;

public:
    // 따라올 위치
    struct TrailPoint
    {
        Vec3 position{};
        Vec3 forward{};
    };

public:
    HRESULT Initialize(uint32 levelIndex, const wstring& layerTag);
    void    Update(float timeDelta);

    // 현재 플레이중인 Hero
    Shared<Player> Get_ActiveHero() const;

    void    Set_TargetCamera(const Shared<TargetCamera>& camera);
    void    Clear_Party();

private:
    void    Switch_NextHero();
    void    Apply_ControlRoles();

    void    Set_ActiveTrail(float timeDelta);
    void    Update_Followers(float timeDelta);

    Vec3    Get_TrailPosition(float delaySeconds) const;

    void    Move_FollowerTo(
                const Shared<Player>& follower,
                const Vec3& targetPos,
                float timeDelta);

    void    Update_FollowerAnimation(
                const Shared<Player>& follower,
                float moveDistance);

    void    Refresh_TargetCamera() const;

private:
    vector<Shared<Player>> _heroes;
    deque<TrailPoint>   _activeTrail;

    Weak<TargetCamera> _targetCamera;

    uint32 _activeIndex = 0;
    uint32 _levelIndex = 0;
    wstring _layerTag;

    float _recordTimer = 0.f;
    float _recordInterval = 0.05f;

    // follower 1은 조금 전 위치, follower 2는 더 이전 위치
    float _followerDelayA = 0.25f;
    float _followerDelayB = 0.50f;

    float _followStopDistance = 0.25f;
    float _followerMoveSpeed = 8.f; // MovementCom에서 입력값 받아오는거로 바꾸긴 해야함

public:
    static Unique<Hero_Manager> Create();
    void Free() override;

};

NS_END
