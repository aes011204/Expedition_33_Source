#include "Hero_Manager.h"

#include "Body_Player.h"
#include "GameInstance.h"
#include "Player.h"
#include "PlayerControllerCom.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "TransformCom.h"

HRESULT Hero_Manager::Initialize_Hero(uint32 levelIndex, const wstring& layerTag)
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

void Hero_Manager::Update_HeroManager(float timeDelta)
{
    if (_heroes.empty())
        return;

    if (GAME->KeyDown(KEY_TYPE::T))
    {
        LOG_DEBUG("Hero Change Apply");

        Switch_NextHero();
    }

    Set_ActiveTrail(timeDelta);
    Update_Followers(timeDelta);
}

Shared<Player> Hero_Manager::Get_ActiveHero() const
{
    if (_heroes.empty() || _activeIndex >= _heroes.size())
        return nullptr;

    return _heroes[_activeIndex];
}

void Hero_Manager::Set_TargetCamera(const Shared<TargetCamera>& camera)
{
     _targetCamera = camera;
    Refresh_TargetCamera();
}

void Hero_Manager::Clear_Party()
{
    _heroes.clear();
    _activeTrail.clear();
    _activeIndex = 0;
    _recordTimer = 0.f;
}

void Hero_Manager::Switch_NextHero()
{
     if (_heroes.size() < 2)
        return;

    const uint32 newIndex =
        (_activeIndex + 1) % static_cast<uint32>(_heroes.size());

    // 전환 전 active Hero의 위치/회전을 기준점으로 세팅
    auto oldActive = Get_ActiveHero();
    if (oldActive == nullptr || oldActive->Get_Transform() == nullptr)
        return;

    auto oldActiveTr = oldActive->Get_Transform();

    const Vec3 anchorPos = oldActiveTr->Get_WorldPosition();
    const Vec3 anchorRot = oldActiveTr->Get_WorldRotationEuler();
    const Vec3 anchorForward = oldActiveTr->Get_WorldForward();
    const Vec3 anchorRight = oldActiveTr->Get_WorldRight();

    _activeIndex = newIndex;

    Apply_ControlRoles();

    // 위치 바로 재배치 -> 근데 지금 셋다 Lune Model이라 티가안나서 로그로만 확인
    Snap_AllHeroesToFormation(anchorPos, anchorRot, anchorForward, anchorRight);

    _activeTrail.clear();
    _recordTimer = 0.f;
    Set_ActiveTrail(_recordInterval);

    Refresh_TargetCamera();
}

void Hero_Manager::Snap_AllHeroesToFormation(const Vec3& anchorPos, const Vec3& anchorRot, Vec3 anchorForward,
    Vec3 anchorRight)
{
    anchorForward.y = 0.f;
    anchorRight.y = 0.f;

    if (anchorForward.LengthSquared() > 0.f)
        anchorForward.Normalize();
    else
        anchorForward = Vec3::Look;

    if (anchorRight.LengthSquared() > 0.f)
        anchorRight.Normalize();
    else
        anchorRight = Vec3::Right;

    uint32 followerSlot = 0;

    for (uint32 i = 0; i < _heroes.size(); ++i)
    {
        auto hero = _heroes[i];
        if (hero == nullptr || hero->Get_Transform() == nullptr)
            continue;

        auto tr = hero->Get_Transform();

        if (i == _activeIndex)
        {
            // 새 active는 이전 active가 있던 메인 자리로 즉시 이동
            tr->Set_WorldPosition(anchorPos);
            tr->Set_WorldRotationEuler(anchorRot);
            continue;
        }

        const float backDistance =
            followerSlot == 0 ? _snapBackDistanceA : _snapBackDistanceB;

        const float sideOffset =
            followerSlot == 0 ? -_followerSideOffset : _followerSideOffset;

        const Vec3 followerPos =
            anchorPos - anchorForward * backDistance + anchorRight * sideOffset;

        // follower도 목표 위치까지 걸어가는 게 아니라 즉시 배정
        tr->Set_WorldPosition(followerPos);
        tr->Set_WorldRotationEuler(anchorRot);

        Update_FollowerAnimation(hero, 0.f);

        ++followerSlot;
    }
}

void Hero_Manager::Apply_ControlRoles()
{
    for (uint32 i = 0; i < _heroes.size(); ++i)
    {
        auto hero = _heroes[i];
        if (!hero)
            continue;

        auto controller = hero->Get_Component<PlayerControllerCom>();
        if (!controller)
            continue;

        bool checkIndex = i == _activeIndex;
        controller->Set_ControlEnabled(checkIndex);
    }
}

void Hero_Manager::Set_ActiveTrail(float timeDelta)
{
    _recordTimer += timeDelta;
    if (_recordTimer < _recordInterval)
        return;

    _recordTimer = 0.f;

    auto active = Get_ActiveHero();
    if (active == nullptr)
        return;

    auto activeTr = active->Get_Transform();
    if (activeTr == nullptr)
        return;

    TrailPoint point{};
    point.position = activeTr->Get_WorldPosition();
    point.forward = activeTr->Get_WorldForward();

    // 최신 위치가 앞으로 오게
    _activeTrail.push_front(point);

    const size_t maxTrailCount = 60;
    while (_activeTrail.size() > maxTrailCount)
        _activeTrail.pop_back();
}

void Hero_Manager::Update_Followers(float timeDelta)
{
    uint32 followerSlot = 0;

    for (uint32 i = 0; i < _heroes.size(); i++)
    {
        if (i == _activeIndex)
            continue;

        const float delay = followerSlot == 0 ? _followerDelayA : _followerDelayB;
        const TrailPoint trailPoint = Get_TrailPoint(delay);

        Vec3 forward = trailPoint.forward;
        forward.y = 0.f;

        if (forward.LengthSquared() > 0.f)
            forward.Normalize();
        else
            forward = Vec3::Look;

        Vec3 right = Vec3::Up.Cross(forward);

        if (right.LengthSquared() > 0.f)
            right.Normalize();
        else
            right = Vec3::Right;

        // 0번은 왼쪽 뒤, 1번은 오른쪽 뒤
        const float backDistance =
            followerSlot == 0 ? _snapBackDistanceA : _snapBackDistanceB;

        const float sideOffset = followerSlot == 0 ? -_followerSideOffset : _followerSideOffset;
        const Vec3 targetPos =
            trailPoint.position - forward * backDistance + right * sideOffset;

        Move_FollowerTo(_heroes[i], targetPos, timeDelta);

        ++followerSlot;
    }

}

Hero_Manager::TrailPoint Hero_Manager::Get_TrailPoint(float delaySeconds) const
{
    if (_activeTrail.empty())
    {
        TrailPoint point{};

        auto active = Get_ActiveHero();
        if (active && active->Get_Transform())
        {
            auto a = active->Get_Transform();
            point.position = a->Get_WorldPosition();
            point.forward = a->Get_WorldForward();
        }

        return point;
    }

    const uint32 index = static_cast<uint32>(delaySeconds / _recordInterval);
    const uint32 clampedIndex = min<uint32>(index, static_cast<uint32>(_activeTrail.size() - 1));

    return _activeTrail[clampedIndex];
}

void Hero_Manager::Move_FollowerTo(const Shared<Player>& follower, const Vec3& targetPos, float timeDelta)
{
    if (follower == nullptr)
        return;

    auto tr = follower->Get_Transform();
    if (tr == nullptr)
        return;

    Vec3 currentPos = tr->Get_WorldPosition();
    Vec3 delta = targetPos - currentPos;
    delta.y = 0.f;

    const float distance = delta.Length();

    if (distance <= _followStopDistance)
    {
        Update_FollowerAnimation(follower, 0.f);
        return;
    }

    delta.Normalize();

    const float moveStep = min(distance, _followerMoveSpeed * timeDelta);
    Vec3 nextPos = currentPos + delta * moveStep;
    nextPos.y = targetPos.y;
    tr->Set_WorldPosition(nextPos);

    // 일단 이동방향 바라보게 임시용
    tr->LookAt(nextPos + delta);

    Update_FollowerAnimation(follower, moveStep);
}

void Hero_Manager::Update_FollowerAnimation(const Shared<Player>& follower, float moveDistance)
{
    if (follower == nullptr)
        return;

    // 애니메이션이 body 기준으로 재생돼서
    auto body = dynamic_cast<Body_Player*>(
        follower->Get_PartObject(TEXT("Part_Body")));

    if (body == nullptr)
        return;

    using Anim = Body_Player::PlayerAnimation;

    if (moveDistance <= 0.001f)
    {
        const uint32 current = body->Get_AnimationIndex();

        if (current != ETOI(Anim::Pl_Idle_0) &&
            current != ETOI(Anim::Pl_Idle_1))
        {
            body->Set_Animation(ETOI(Anim::Pl_Idle_0), 0.1f, true);
        }

        return;
    }

    if (body->Get_AnimationIndex() != ETOI(Anim::Pl_Run))
        body->Set_Animation(ETOI(Anim::Pl_Run), 0.1f, true);
}

void Hero_Manager::Refresh_TargetCamera() const
{
    auto camera = _targetCamera.lock();
    auto active = Get_ActiveHero();

    if (camera == nullptr || active == nullptr)
        return;

    camera->SetTargetTransform(active->Get_Transform());
}

Unique<Hero_Manager> Hero_Manager::Create()
{
    auto instance = make_unique<Hero_Manager>();
    //instance->Initialize_Hero();

    return instance;
}

void Hero_Manager::Free()
{
    Base::Free();
}
