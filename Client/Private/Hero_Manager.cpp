#include "Hero_Manager.h"
#include "Body_Player.h"
#include "Client_Constants.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "Player_WorldBase.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "TransformCom.h"
#include "ColliderCom.h"
#include "PhysX_Types.h"
#include "MovementCom.h"

static const HeroSpawnDesc heroSpawnDescs[] =
{
    { HeroId::Gustave, Constants::Prefab::PW_Gustave, Constants::Prefab::PB_Gustave }, // 월드/배틀 prefab 이름을 한 곳에서 관리한다.
    { HeroId::Lune,    Constants::Prefab::PW_Lune,    Constants::Prefab::PB_Lune    }, // Player/Battle 구 이름이 호출부에 남지 않게 한다.
    { HeroId::Maelle,  Constants::Prefab::PW_Maelle,  Constants::Prefab::PB_Maelle  }, // 파일명 변경과 스폰 이름을 같은 상수로 묶는다.
};

void Hero_Manager::Change_GameMenuAnimation(uint32 introAnim, uint32 idleAnim)
{
    using plAnim = Body_Player::PlayerWorldAnimation;

    for (uint32 i = 0; i < static_cast<uint32>(_heroes.size()); ++i)
    {
        if (_activeIndex == i)
        {
            _heroes[i].hero->Enter_GameMenuAnimation(introAnim, idleAnim);
        }
        else
        {
            _heroes[i].hero->Enter_GameMenuAnimation(
                ETOI(plAnim::Pl_GameMenu_Intro),
                ETOI(plAnim::Pl_GameMenu_Unselected));
        }
    }
}

HRESULT Hero_Manager::Initialize_Hero(uint32 levelIndex, const wstring& layerTag, const Vec3& startPos)
{
    Clear_Party();

    _levelIndex = levelIndex;
    _layerTag = layerTag;
    _activeIndex = 0;

    for (uint32 i = 0; i < 3; ++i)
    {
        const HeroSpawnDesc& desc = heroSpawnDescs[i];

        auto object = Spawn_Prefab::Prefab(desc.worldPrefabName)
            .AtLevel(levelIndex)
            .InLayer(layerTag)
            .Spawn();

        auto hero = dynamic_pointer_cast<Player_WorldBase>(object);
        CHECK_NULL(hero, E_FAIL);

        Vec3 pos = startPos;
        pos.z -= static_cast<float>(i) * 1.5f;

        hero->Get_Transform()->Set_WorldPosition(pos);
        _heroes.push_back(HeroPartyEntry { desc.heroId, hero });
    }

    Apply_ControlRoles();

    // 시작 직후에 active 위치 한번 기록
    Set_ActiveTrail(_recordInterval);
    Refresh_TargetCamera();

    return S_OK;
}

void Hero_Manager::Update_HeroManager(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return; // 배틀 중에서는 월드 플레이어 입력 제한

    if (_heroes.empty())
        return;

    auto state = Get_ActiveHero()->Get_Component<PlayerStateMachineCom>();
    CHECK_NULL(state);

    if (GAME->KeyDown(KEY_TYPE::T))
    {
        // 점프 도중에는 막기
        if (state->Get_CurrentPlayerState() == PlayerState::Jump)
            return;

        Switch_NextHero();
    }

    Set_ActiveTrail(timeDelta);
    Update_Followers(timeDelta);
}

Shared<Player_WorldBase> Hero_Manager::Get_ActiveHero() const
{
    if (_heroes.empty() || _activeIndex >= _heroes.size())
        return nullptr;

    return _heroes[_activeIndex].hero;
}

void Hero_Manager::Set_TargetCamera(const Shared<TargetCamera>& camera)
{
     _targetCamera = camera;
    Refresh_TargetCamera();
}

void Hero_Manager::Clear_Party()
{
    Reset_AllHeroesToIdle();

    for (auto& entry : _heroes)
    {
        if (!entry.hero)
            continue;

        auto movement = entry.hero->Get_Component<MovementCom>();
        if (movement)
        {
            movement->Clear_FollowMoveCommand();
            movement->Apply_Input({});
        }
    }

    _heroes.clear();
    _activeTrail.clear();
    _activeIndex = 0;
    _recordTimer = 0.f;
}

const vector<HeroPartyEntry>& Hero_Manager::Get_HeroEntries() const
{
    return _heroes;
}

Shared<Player_WorldBase> Hero_Manager::Get_Hero(HeroPartySlot slot) const
{
    const uint32 index = ETOI(slot);

    if (index >= _heroes.size())
        return nullptr;

    return _heroes[index].hero;
}

HeroId Hero_Manager::Get_HeroId(HeroPartySlot slot) const
{
    const uint32 index = ETOI(slot);

    if (index >= _heroes.size())
        return HeroId::END;

    return _heroes[index].heroId;
}

HeroPartySlot Hero_Manager::Get_ActiveHeroSlot() const
{
    return static_cast<HeroPartySlot>(_activeIndex);
}

Shared<Player_WorldBase> Hero_Manager::Get_FrontHero() const
{
    if (_heroes.empty())
        return nullptr;

    return _heroes.front().hero;
}

Shared<Player_WorldBase> Hero_Manager::Find_Hero(HeroId heroId) const
{
    for (const auto& entry : _heroes)
    {
        if (entry.heroId == heroId)
            return entry.hero;
    }

    return nullptr;
}

const HeroSpawnDesc* Hero_Manager::Find_HeroSpawnDesc(HeroId heroId)
{
     for (const HeroSpawnDesc& desc : heroSpawnDescs)
    {
        if (desc.heroId == heroId)
            return &desc;
    }

    return nullptr;
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

    Snap_AllHeroesToFormation(anchorPos, anchorRot, anchorForward, anchorRight);

    for (uint32 i = 0; i < _heroes.size(); ++i)
    {
        if (i == _activeIndex)
            continue;

        if (const Shared<Player_WorldBase>& hero = _heroes[i].hero)
        {
            hero->Start_SwitchDissolve(0.35f, "T_Perlin_Noise_M");
        }
    }

    for (uint32 i = 0; i < _heroes.size(); ++i)
    {
        _followerRuntime[i] = {};
    }

    Reset_AllHeroesToIdle(); // 전환 직후 모든 hero를 Idle 상태로 정리

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
        auto hero = _heroes[i].hero;
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

        // 전환 후 배치 전에 PhysX 벽 파고들기 바닥 높이 보정 후 배치
        const Vec3 safeFollowerPos = Resolve_SnapPosition(
            hero, tr->Get_WorldPosition(), followerPos);

        // follower도 목표 위치까지 걸어가는 게 아니라 즉시 배정
        tr->Set_WorldPosition(safeFollowerPos);
        tr->Set_WorldRotationEuler(anchorRot);

        auto movement = hero->Get_Component<MovementCom>();
        if (movement)
        {
            movement->Clear_FollowMoveCommand();
            movement->Apply_Input({});
            movement->Stop_AllMovement();
        }

        auto controller = hero->Get_Component<PlayerControllerCom>();
        if (controller)
            controller->Clear_FollowControl();

        ++followerSlot;
    }
}

Vec3 Hero_Manager::Resolve_SnapPosition(const Shared<Player_WorldBase>& hero, const Vec3& fromPos, const Vec3& desiredPos) const
{
    if (hero == nullptr)
        return desiredPos;

    auto collider = hero->Get_Component<ColliderCom>();
    if (collider == nullptr || collider->Get_PxShape() == nullptr)
        return desiredPos;

    auto movement = hero->Get_Component<MovementCom>();

    float skinWidth = 0.03f;
    float groundSnapUp = 0.35f;
    float groundSnapDown = 0.65f;

    if (movement)
    {
        const auto& moveDesc = movement->Get_MoveDesc();

        skinWidth = moveDesc.sweep.skinWidth;         
        groundSnapUp = moveDesc.sweep.groundSnapUp;       // 위에서부터 바닥 probe 시작
        groundSnapDown = moveDesc.sweep.groundSnapDown;   // 아래로 탐색할 거리
    }

    Vec3 sweepTarget = desiredPos;
    sweepTarget.y = fromPos.y;

    Vec3 sweep = sweepTarget - fromPos;
    const float sweepDistance = sweep.Length();

    // 후보 위치 설정
    Vec3 candidatePos = desiredPos;

    if (sweepDistance > 0.0001f)
    {
        Vec3 sweepDir = sweep / sweepDistance;

        Matrix sweepWorld = hero->Get_Transform()->Get_WorldMatrix();
        sweepWorld.Translation(fromPos);

        PhysXQueryHit hit{};
        const bool blocked = GAME->Sweep_Collider(
                collider.get(),
                sweepWorld,
                sweepDir,
                sweepDistance,
                hit);

        if (blocked)
        {
            const float safeDistance = max(0.f, hit.distance - skinWidth);

            candidatePos = fromPos + sweepDir * safeDistance; // 벽 앞까지만 밀어서 배치
            candidatePos.y = desiredPos.y;
        }
    }

    Matrix probeWorld = hero->Get_Transform()->Get_WorldMatrix();
    // 후보 위치에서 아래 탐색
    probeWorld.Translation(candidatePos + Vec3(0.f, groundSnapUp, 0.f));

    PhysXQueryHit groundHit{};
    const float probeDistance = groundSnapUp + groundSnapDown;

    const bool hasGround = GAME->Probe_Ground(
        collider.get(),
        probeWorld,
        probeDistance,
        groundHit);

    // 바닥 위로 높이 보정
    if (hasGround)
        candidatePos.y += groundSnapUp - max(0.f, groundHit.distance - skinWidth);

    return candidatePos;
}

Vec3 Hero_Manager::Resolve_FollowTargetPosition(const Shared<Player_WorldBase>& hero, const Vec3& fromPos, const Vec3& desiredPos) const
{
    if (hero == nullptr)
        return desiredPos;

    auto collider = hero->Get_Component<ColliderCom>();
    if (collider == nullptr || collider->Get_PxShape() == nullptr)
        return desiredPos;

    auto movement = hero->Get_Component<MovementCom>();

    float skinWidth = 0.03f;
    float groundSnapUp = 0.35f;
    float groundSnapDown = 0.65f;

    if (movement)
    {
        const auto& moveDesc = movement->Get_MoveDesc();

        skinWidth = moveDesc.sweep.skinWidth;
        groundSnapUp = moveDesc.sweep.groundSnapUp;
        groundSnapDown = moveDesc.sweep.groundSnapDown;
    }

    Vec3 candidatePos = desiredPos;
    candidatePos.y = fromPos.y;

    Matrix probeWorld = hero->Get_Transform()->Get_WorldMatrix();
    probeWorld.Translation(candidatePos + Vec3(0.f, groundSnapUp, 0.f));

    PhysXQueryHit groundHit{};
    const float probeDistance = groundSnapUp + groundSnapDown;

    const bool hasGround = GAME->Probe_Ground(
        collider.get(),
        probeWorld,
        probeDistance,
        groundHit);

    if (hasGround)
        candidatePos.y += groundSnapUp - max(0.f, groundHit.distance - skinWidth);

    return candidatePos;
}

void Hero_Manager::Apply_ControlRoles()
{
    for (uint32 i = 0; i < _heroes.size(); ++i)
    {
        auto hero = _heroes[i].hero;
        if (!hero)
            continue;

        auto controller = hero->Get_Component<PlayerControllerCom>();
        if (!controller)
            continue;

        const bool isActive = (i == _activeIndex);
        controller->Set_ControlEnabled(isActive);

        auto movement = hero->Get_Component<MovementCom>();
        if (movement)
        {
            movement->Clear_FollowMoveCommand();
            movement->Apply_Input({});
            movement->Set_MoveBackend(MovementCom::MoveBackend::PhysXCCT);
        }

        if (isActive)
            controller->Clear_FollowControl();
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
        const Vec3 rawTargetPos =
            trailPoint.position - forward * backDistance + right * sideOffset;

        auto& runtime = _followerRuntime[i];
        Vec3 targetPos = rawTargetPos;

        if (_heroes[i].hero && _heroes[i].hero->Get_Transform())
        {
            const Vec3 currentPos = _heroes[i].hero->Get_Transform()->Get_WorldPosition();
            targetPos = Resolve_FollowTargetPosition(_heroes[i].hero, currentPos, rawTargetPos);
            runtime.desiredTargetPos = targetPos;

            const Vec3 delta = targetPos - currentPos;
            runtime.lastMoveDistance = delta.Length();

            runtime.sprintFollowHoldTime = max(0.f, runtime.sprintFollowHoldTime - timeDelta);

            if (!runtime.sprintFollow && runtime.lastMoveDistance >= _sprintFollowDistance)
            {
                runtime.sprintFollow = true;
                runtime.sprintFollowHoldTime = _sprintFollowMinHoldTime;
            }
            else if (
                runtime.sprintFollow &&
                runtime.sprintFollowHoldTime <= 0.f &&
                runtime.lastMoveDistance <= _sprintFollowReleaseDistance)
            {
                runtime.sprintFollow = false;
            }
        }
        else
        {
            runtime.desiredTargetPos = targetPos;
            runtime.lastMoveDistance = 0.f;
            runtime.sprintFollow = false;
            runtime.sprintFollowHoldTime = 0.f;
        }

        Move_FollowerTo(_heroes[i].hero, i, targetPos, timeDelta);

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

void Hero_Manager::Move_FollowerTo(const Shared<Player_WorldBase>& follower, uint32 followerIndex, const Vec3& targetPos, float timeDelta)
{
    if (follower == nullptr)
        return;

    auto controller = follower->Get_Component<PlayerControllerCom>();
    CHECK_NULL(controller);

    auto movement = follower->Get_Component<MovementCom>();
    CHECK_NULL(movement);

    auto tr = follower->Get_Transform();
    CHECK_NULL(tr);

    const Vec3 currentPos = tr->Get_WorldPosition();
    const Vec3 delta = targetPos - currentPos;
    const float distance = delta.Length();

    if (distance >= _teleportReseatDistance)
    {
        tr->Set_WorldPosition(targetPos); // 너무 멀어지면, follow가 쓰던 같은 지면 보정 위치로 재배치

        movement->Clear_FollowMoveCommand();
        movement->Apply_Input({});
        movement->Stop_AllMovement();

        controller->Clear_FollowControl(); // 재배치 후, follower 상태 초기화
        return;
    }

    PlayerControllerCom::FollowControl follow{};
    follow.enabled = true;

    Vec3 moveDelta = delta;
    moveDelta.y = 0.f;

    if (moveDelta.Length() > _followStopDistance)
    {
        moveDelta.Normalize();

        follow.hasMoveIntent = true;
        follow.worldDir = moveDelta;
        follow.sprintFollow = _followerRuntime[followerIndex].sprintFollow;
    }

    controller->Set_FollowControl(follow);
}

void Hero_Manager::Refresh_TargetCamera() const
{
    auto camera = _targetCamera.lock();
    auto active = Get_ActiveHero();

    if (camera == nullptr || active == nullptr)
        return;

    camera->SetTargetTransform(active->Get_Transform());
}

HRESULT Hero_Manager::Ready_Delegates()
{
    _editorStopHandle = GAME->Get_DelegateHub().OnEditorStopRuntime.Add(
        [this]()
        {
           Handle_EditorStopRuntime(); 
        });

    return S_OK;
}

void Hero_Manager::Reset_AllHeroesToIdle()
{
    for (auto& entry : _heroes)
    {
        auto hero = entry.hero;
        if (!hero)
            continue;

        auto controller = hero->Get_Component<PlayerControllerCom>();
        if (controller)
            controller->Clear_FollowControl();

        auto movement = hero->Get_Component<MovementCom>();
        if (movement)
        {
            movement->Clear_FollowMoveCommand(); 
            movement->Apply_Input({});           
            movement->Stop_HorizontalMovement(); 
        }

        auto state = hero->Get_Component<PlayerStateMachineCom>();
        if (state)
            state->Change_State(PlayerState::Idle); // 전환/종료 시 월드 기본 상태로 돌린다.
    }
}

void Hero_Manager::Handle_EditorStopRuntime()
{
    Reset_AllHeroesToIdle();
}

bool Hero_Manager::Is_ActiveHero(const Shared<GameObject>& target) const
{
    if (target == nullptr)
        return false;

    const Shared<Player_WorldBase> activeHero = Get_ActiveHero();
    if (activeHero == nullptr)
        return false;

    return target == static_pointer_cast<GameObject>(activeHero);
}

Unique<Hero_Manager> Hero_Manager::Create()
{
    auto instance = make_unique<Hero_Manager>();

    if (FAILED(instance->Ready_Delegates()))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

void Hero_Manager::Free()
{
    if (_editorStopHandle.IsValid())
    {
        GAME->Get_DelegateHub().OnEditorStopRuntime.Remove(_editorStopHandle);
        _editorStopHandle.Reset();
    }

    Base::Free();
}
