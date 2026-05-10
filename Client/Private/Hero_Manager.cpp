#include "Hero_Manager.h"

#include "Body_Player.h"
#include "GameInstance.h"
#include "Player.h"
#include "PlayerControllerCom.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "TransformCom.h"

HRESULT Hero_Manager::Initialize(uint32 levelIndex, const wstring& layerTag)
{
    Clear_Party();

    _levelIndex = levelIndex;
    _layerTag = layerTag;
    _activeIndex = 0;

    const Vec3 startPos(10.f, 5.f, 10.f);

    for (uint32 i = 0; i < 3; ++i)
    {
        auto object = Spawn_Prefab::Prefab("TestPlayer")
            .AtLevel(levelIndex)
            .InLayer(layerTag)
            .Spawn();

        auto hero = dynamic_pointer_cast<Player>(object);
        CHECK_NULL(hero, E_FAIL);

        // 초기 위치 설정
        Vec3 pos = startPos;
        pos.z -= static_cast<float>(i) * 1.5f;

        hero->Get_Transform()->Set_WorldPosition(pos);
        _heroes.push_back(hero);
    }

    Apply_ControlRoles();

    // 시작 직후에 active 위치 한번 기록
    Set_ActiveTrail(_recordInterval);
    Refresh_TargetCamera();

    return S_OK;
}

void Hero_Manager::Update(float timeDelta)
{
    if (_heroes.empty())
        return;

    if (GAME->KeyDown(KEY_TYPE::T))
    {
        Switch_NextHero();
    }

    Set_ActiveTrail(timeDelta);
    Update_Followers(timeDelta);
}

Shared<Player> Hero_Manager::Get_ActiveHero() const
{
    
}

void Hero_Manager::Set_TargetCamera(const Shared<TargetCamera>& camera)
{
}

void Hero_Manager::Clear_Party()
{
}

void Hero_Manager::Switch_NextHero()
{
    if (_heroes.size() < 2)
        return;

    const uint32 oldIndex = _activeIndex;
}

void Hero_Manager::Apply_ControlRoles()
{
}

void Hero_Manager::Set_ActiveTrail(float timeDelta)
{
}

void Hero_Manager::Update_Followers(float timeDelta)
{
}

Vec3 Hero_Manager::Get_TrailPosition(float delaySeconds) const
{
}

void Hero_Manager::Move_FollowerTo(const Shared<Player>& follower, const Vec3& targetPos, float timeDelta)
{
}

void Hero_Manager::Update_FollowerAnimation(const Shared<Player>& follower, float moveDistance)
{
}

void Hero_Manager::Refresh_TargetCamera() const
{
}

Unique<Hero_Manager> Hero_Manager::Create()
{
    
}

void Hero_Manager::Free()
{
    Base::Free();
}
