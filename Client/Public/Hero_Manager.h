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

    struct FollowerRuntime
    {
        Vec3    desiredTargetPos = Vec3::Zero; // follower 목표 위치
        float   lastMoveDistance = 0.f;        // 목표까지 남은 거리
        bool    catchUp = false;               // 추격 이동 여부
        float   catchUpHoldTime = 0.f;         // Sprint 유지 시간. Run으로 바로 안바뀌게
    };

public:
    HRESULT Initialize_Hero(
        uint32 levelIndex,
        const wstring& layerTag,
        const Vec3& startPos = Vec3::Zero);

    void    Update_HeroManager(float timeDelta);



    void    Set_TargetCamera(const Shared<TargetCamera>& camera);
    void    Clear_Party();

    // 셋 다 가져오기
    const vector<HeroPartyEntry>& Get_HeroEntries() const;

    Shared<Player> Get_Hero(HeroPartySlot slot) const;
    HeroId Get_HeroId(HeroPartySlot slot) const;

    // 현재 플레이중인 Hero
    HeroPartySlot Get_ActiveHeroSlot() const;
    Shared<Player> Get_ActiveHero() const;

    // 스테이터스 찍을 때 Front로 들어간 구스타브 포커싱되게
    Shared<Player> Get_FrontHero() const;

    Shared<Player> Find_Hero(HeroId heroId) const;

private:
    void    Switch_NextHero();
    void    Snap_AllHeroesToFormation(
                const Vec3& anchorPos,
                const Vec3& anchorRot,
                Vec3 anchorForward,
                Vec3 anchorRight);

    void    Apply_ControlRoles();

    void    Set_ActiveTrail(float timeDelta);
    void    Update_Followers(float timeDelta);

    TrailPoint    Get_TrailPoint(float delaySeconds) const;

    void    Move_FollowerTo(
                const Shared<Player>& follower,
                uint32 followerIndex,
                const Vec3& targetPos,
                float timeDelta);

    void    Update_FollowerAnimation(
                const Shared<Player>& follower,
                float moveDistance, bool useSprint);

    void    Refresh_TargetCamera() const;

private:
    vector<HeroPartyEntry>  _heroes;
    uint32                  _activeIndex = 0;

    deque<TrailPoint>       _activeTrail;

    Weak<TargetCamera>      _targetCamera;

    uint32 _levelIndex = 0;
    wstring _layerTag;

    float _recordTimer = 0.f;
    float _recordInterval = 0.02f;

    // follower 1은 조금 전 위치, follower 2는 더 이전 위치
    float _followerDelayA = 0.20f;
    float _followerDelayB = 0.35f;

    float _followerSideOffset = 1.4f;

    float _followStopDistance = 0.25f;
    float _followerMoveSpeed = 8.f; // MovementCom에서 입력값 받아오는거로 바꾸긴 해야함

    float _snapBackDistanceA = 2.0f;
    float _snapBackDistanceB = 3.0f;

    array<FollowerRuntime, 3> _followerRuntime{};
    float _catchUpDistance = 2.5f;
    float _catchUpReleaseDistance = 1.2f;   // sprint 해제는 더 가까워졌을 때만
    float _catchUpMinHoldTime = 0.25f;      // sprint에 들어가면 최소 이 시간은 유지
    float _teleportReseatDistance  = 6.f;

public:
    static Unique<Hero_Manager> Create();
    void Free() override;

};

NS_END
