#include "Monster_WorldBase.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "ClientInstance.h"
#include "Client_Constants.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "MonsterStatCom.h"
#include "Bounding_OBB.h"
#include "ClientScreenFxManager.h"
#include "MovementCom.h"
#include "Monster_Part.h"
#include "BattleStartSpawner.h"
#include "EffectCom.h"
#include "PlayerStateMachineCom.h"
#include "Mon_WeakPoint.h"
#include "BattleEncounterProfile.h"

REGISTER_GLOBAL_ENUM(Monster_WorldBase::MonsterWorldAnim, "MonsterWorldAnim")

namespace
{
    constexpr wchar_t kWorldActionHitEffect[] =
        L"Effects\\Assets\\Player\\Lune\\Lune_Immolation_Hit.effect.json";

    constexpr wchar_t kWorldActionDistortionEffect[] =
        L"Effects\\Assets\\Distortion\\DIstortion_SuddenAttack.effect.json";

    json Vec3_ToJson(const Vec3& value)
    {
        return json::array({ value.x, value.y, value.z });
    }

    bool Json_ToVec3(const json& value, Vec3& outVec3)
    {
        if (!value.is_array() || value.size() != 3)
            return false;

        outVec3.x = value[0].get<float>();
        outVec3.y = value[1].get<float>();
        outVec3.z = value[2].get<float>();
        return true;
    }

    json BattleMonsterSpawnDesc_ToJson(const Client::BattleMonsterSpawnDesc& desc)
    {
        return {
            { "prefab_name", desc.prefabName },
            { "offset", Vec3_ToJson(desc.offset) },
            { "rotation", Vec3_ToJson(desc.rotation) },
            { "wide_back_offset", desc.wideBackOffset },
            { "wide_height_offset", desc.wideHeightOffset },
            { "wide_look_height_offset", desc.wideLookHeightOffset },
        };
    }

    bool Json_ToBattleMonsterSpawnDesc(const json& value, Client::BattleMonsterSpawnDesc& outDesc)
    {
        if (!value.is_object())
            return false;

        outDesc = {};
        outDesc.prefabName = value.value("prefab_name", "");

        if (value.contains("offset"))
            (void)Json_ToVec3(value["offset"], outDesc.offset);

        if (value.contains("rotation"))
            (void)Json_ToVec3(value["rotation"], outDesc.rotation);

        outDesc.wideBackOffset = value.value("wide_back_offset", 0.f);
        outDesc.wideHeightOffset = value.value("wide_height_offset", 0.f);
        outDesc.wideLookHeightOffset = value.value("wide_look_height_offset", 0.f);

        return true;
    }

    string Resolve_SourceBattlePrefabName(const string& sourcePrefabName)
    {
        if (sourcePrefabName.starts_with("MW_") || sourcePrefabName.starts_with("TW_"))
            return "MB_" + sourcePrefabName.substr(3); // MW_Lancelier/TW_Lancelier -> MB_Lancelier로 데이터 전달

        return "";
    }
}

Monster_WorldBase::Monster_WorldBase(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character_WorldBase(device, context)
{
}

Monster_WorldBase::Monster_WorldBase(const Monster_WorldBase& prototype)
    : Character_WorldBase(prototype)
{
}

HRESULT Monster_WorldBase::Initialize_Prototype()
{
    return Character_WorldBase::Initialize_Prototype();
}

HRESULT Monster_WorldBase::Initialize(void* arg)
{
    CHECK_FAILED(Character_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);



    return S_OK;
}

void Monster_WorldBase::BeginPlay()
{
    Character_WorldBase::BeginPlay();
    _hasAppliedAction = false;

    if (_movementCom)
    {
        _movementCom->Sync_RuntimePositionToTransform();
        _movementCom->Stop_AllMovement();
        _movementCom->Clear_FollowMoveCommand();
        _movementCom->Apply_Input({});
    }

    if (!_editorStopHandle.IsValid())
        _editorStopHandle = GAME->Get_DelegateHub().OnEditorStopRuntime.Add(this, &Monster_WorldBase::On_EditorStopRuntime);

    if (auto* body = dynamic_cast<Monster_Part*>(Get_PartObject(L"Part_Body")))
    {
        if (Shared<ModelCom> model = body->Get_Model())
        {
            model->Refresh_AnimStateMappingFromOwnerContext();
        }
    }

}

void Monster_WorldBase::Update(float timeDelta)
{
    if (CLIENT->Is_WorldControlSuspended())
        return;

    if (_combatStat)
        _combatStat->Update(timeDelta);

    auto* bodyPart = dynamic_cast<Monster_Part*>(Get_PartObject(L"Part_Body"));
    if (_isCinematicDriven)
    {
        if (bodyPart)
        {
            bodyPart->Play_Animation(timeDelta);
            _transformCom->Set_SpeedPerSec(0.f);
        }

        Character_WorldBase::Update(timeDelta);

        if (_effectCom)
            _effectCom->Update(timeDelta);

        return;
    }

    const Shared<GameObject> player = CLIENT->Find_Player(GAME->Current_LevelIndex());
    bool holdStartIdle = false;

    if (_startIdleUntilPlayerInRange && !_hasReleasedStartIdle)
    {
        if (player == nullptr || _transformCom == nullptr || _startIdleReleaseDistance <= 0.f)
        {
            holdStartIdle = true; // 맵 진입 직후 player/transform이 아직 안정되지 않은 프레임에도 patrol 진입을 막는다.
        }
        else
        {
            const float distToPlayer = (player->Get_Transform()->Get_Position() - _transformCom->Get_Position()).Length();
            if (distToPlayer <= _startIdleReleaseDistance)
                _hasReleasedStartIdle = true; // 플레이어가 충분히 가까워진 뒤에는 다시 원래 BT를 그대로 사용한다.
            else
                holdStartIdle = true;
        }
    }

    if (_currentAction != MonsterWorldAnim::Hit)
    {
        if (_isStun)
            _currentAction = MonsterWorldAnim::Stun;
        else
            _currentAction = _desiredAction;
    }


    //if (_aiCtrl && _aiCtrl->Get_IsMoving())
    //    Set_CurrentAction(MonsterAction::Move);

    if (_currentAction == MonsterWorldAnim::Hit && bodyPart && bodyPart->Is_AnimationFinished())
    {
        OnHitAnimationFinished();
    }

    if (bodyPart)
    {
        if (!_hasAppliedAction || _currentAction != _previousAction)
        {
            //LOG_DEBUG("Monster_WorldBase action change current={} previous={}",
            //    ETOI(_currentAction),
            //    ETOI(_previousAction));

            bool applied = false;
            const bool isInitialActionApply = !_hasAppliedAction;

            switch (_currentAction)
            {
            case MonsterWorldAnim::Hit:
                if (isInitialActionApply)
                    applied = bodyPart->Get_Model() && bodyPart->Get_Model()->Set_AnimationByState(ETOI(MonsterWorldAnim::Hit), false);
                else
                    applied = bodyPart->Set_AnimationByState(ETOI(MonsterWorldAnim::Hit), 0.2f, false);
                break;

            case MonsterWorldAnim::Stun:
                if (isInitialActionApply)
                    applied = bodyPart->Get_Model() && bodyPart->Get_Model()->Set_AnimationByState(ETOI(MonsterWorldAnim::Stun), true);
                else
                    applied = bodyPart->Set_AnimationByState(ETOI(MonsterWorldAnim::Stun), 0.3f, true);
                break;

            case MonsterWorldAnim::Idle:
                if (isInitialActionApply)
                    applied = bodyPart->Get_Model() && bodyPart->Get_Model()->Set_AnimationByState(ETOI(MonsterWorldAnim::Idle), true);
                else
                    applied = bodyPart->Set_AnimationByState(ETOI(MonsterWorldAnim::Idle), 0.3f, true);
                break;

            case MonsterWorldAnim::Patrol:
                if (isInitialActionApply)
                    applied = bodyPart->Get_Model() && bodyPart->Get_Model()->Set_AnimationByState(ETOI(MonsterWorldAnim::Patrol), true);
                else
                    applied = bodyPart->Set_AnimationByState(ETOI(MonsterWorldAnim::Patrol), 0.3f, true);
                break;

            case MonsterWorldAnim::Chase:
                if (isInitialActionApply)
                    applied = bodyPart->Get_Model() && bodyPart->Get_Model()->Set_AnimationByState(ETOI(MonsterWorldAnim::Chase), true);
                else
                    applied = bodyPart->Set_AnimationByState(ETOI(MonsterWorldAnim::Chase), 0.3f, true);
                break;
            }

            if (Shared<ModelCom> model = bodyPart->Get_Model())
            {
                const char* animName = model->Get_AnimationName(model->Get_CurrentAnimationIndex());
                //LOG_DEBUG("Monster_WorldBase action applied={} currentAction={} currentAnim={}",
                //    applied,
                //    ETOI(_currentAction),
                //    animName != nullptr ? animName : "<null>");
            }


            if (applied)
            {
                _previousAction = _currentAction;
                _hasAppliedAction = true;
            }

        }

        bodyPart->Play_Animation(timeDelta);

        const auto rootVelocity = bodyPart->Get_RootTransformVelocity();
        _transformCom->Set_SpeedPerSec(rootVelocity.position.z * _worldMoveSpeedScale);
    }

    if (_movementCom)
    {
        const float moveSpeed = max(_transformCom->Get_SpeedPerSec(), 0.f);
        _movementCom->Set_WalkSpeed(moveSpeed);
        _movementCom->Set_RunSpeed(moveSpeed);
        _movementCom->Set_SprintSpeed(moveSpeed);

        if (_isStun || _currentAction == MonsterWorldAnim::Hit || !_aiCtrl || !_aiCtrl->Has_MoveIntent())
        {
            _movementCom->Clear_FollowMoveCommand();
            _movementCom->Apply_Input({});
            _movementCom->Stop_HorizontalMovement();
            _movementCom->Update_Movement(timeDelta);
        }
        else
        {
            MovementCom::FollowMoveCommand command{};
            command.enabled = true;
            command.worldDir = _aiCtrl->Get_MoveWorldDir();

            _movementCom->Set_FollowMoveCommand(command);
            _movementCom->Apply_Input({});
            _movementCom->Update_Movement(timeDelta);
        }
    }

    // if (_isStun == true)
    // {
    //     _stunTime -= timeDelta;
    //
    //     if (_stunTime <= 0.f)
    //     {
    //         _isStun = false;
    //
    //     }
    // }

    if (_aiCtrl)
    {
        const Shared<BlackBoard> bb = _aiCtrl->Get_BlackBoard();

        if (holdStartIdle)
        {
            _aiCtrl->Stop_Move();
            _desiredAction = MonsterWorldAnim::Idle; // 시작 Idle 게이트 중에는 patrol/chase가 desiredAction을 덮지 못하게 한다.
        }
        else if (bb && player)
        {
            bb->Set_Value("playerPos", player->Get_Transform()->Get_Position());
            bb->Set_Value("isStun", _isStun);

            _aiCtrl->Update(timeDelta); // 이번 프레임은 기존 desiredAction을 먼저 소비하고, BT 결과는 다음 프레임부터 반영한다.
        }
    }

    Character_WorldBase::Update(timeDelta);

    if (_effectCom)
        _effectCom->Update(timeDelta);
}


void Monster_WorldBase::Apply_Stun(float duration)
{
    if (_isStun == true)
        return;
    _isStun = true;
    _stunTime = 0.f;

    if (_aiCtrl)
        _aiCtrl->Stop_Move();

    _currentAction = MonsterWorldAnim::Hit;
    _hasAppliedAction = false;
}

void Monster_WorldBase::Set_CinematicDriven(bool cinematicDriven)
{
    if (_isCinematicDriven == cinematicDriven)
        return;

    _isCinematicDriven = cinematicDriven;

    if (_aiCtrl)
        _aiCtrl->Stop_Move();

    if (_movementCom)
    {
        _movementCom->Clear_FollowMoveCommand();
        _movementCom->Apply_Input({});
        _movementCom->Stop_AllMovement();
        _movementCom->Stop_VerticalMovement();
        _movementCom->Sync_RuntimePositionToTransform();
    }

    if (_transformCom)
        _transformCom->Set_SpeedPerSec(0.f);
}

void Monster_WorldBase::OnHitAnimationFinished()
{
    _isStun = false;
    _stunTime = 0.f;
    _currentAction = _desiredAction;

    _hasAppliedAction = false;
}

void Monster_WorldBase::Late_Update(float timeDelta)
{
    auto* weakPoint = dynamic_cast<Mon_WeakPoint*>(Get_PartObject(L"Part_Weak"));

    // 배틀 중에는 월드 베이스 안보이게
    if (CLIENT->Is_WorldControlSuspended())
    {
        Set_Visible(false);

        if (weakPoint)
            weakPoint->Hide_WorldWeakPointForBattle();

        return;
    }

    if (weakPoint && !weakPoint->Is_Visible())
        weakPoint->Restore_WorldWeakPointAfterBattle();

    Character_WorldBase::Late_Update(timeDelta);
    Update_Culling(Vec3(1.5f, 4.f, 1.5f));
}

HRESULT Monster_WorldBase::Render()
{
    return Character_WorldBase::Render();
}

HRESULT Monster_WorldBase::Render_Shadow()
{
    return Character_WorldBase::Render_Shadow();
}

json Monster_WorldBase::To_Json() const
{
    json data = Character_WorldBase::To_Json();

    json& customProperties = data["custom_properties"];
    customProperties["battle_start_spawner_id"] = _battleStartSpawnerId;
    customProperties["battle_start_offset"] = Vec3_ToJson(_battleStartOffset);
    customProperties["battle_encounter_id"] = _battleEncounterId;
    customProperties["start_idle_until_player_in_range"] = _startIdleUntilPlayerInRange;
    customProperties["start_idle_release_distance"] = _startIdleReleaseDistance;

    json& battleMonsterSpawns = customProperties["battle_monster_spawns"];
    battleMonsterSpawns = json::array();

    for (const BattleMonsterSpawnDesc& spawnDesc : _battleMonsterSpawnDescs)
        battleMonsterSpawns.push_back(BattleMonsterSpawnDesc_ToJson(spawnDesc));

    return data;
}

void Monster_WorldBase::From_Json(const json& data)
{
    Character_WorldBase::From_Json(data);

    _isBattleStart = false;
    _isStun = false;
    _stunTime = 0.f;
    _desiredAction = MonsterWorldAnim::Idle;
    _currentAction = MonsterWorldAnim::Idle;
    _previousAction = MonsterWorldAnim::Idle;
    _hasAppliedAction = false;
    _startIdleUntilPlayerInRange = false;
    _hasReleasedStartIdle = false;
    _startIdleReleaseDistance = 0.f;

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(true);
        _colliderCom->Clear_Overlap();
    }

    if (_movementCom)
    {
        _movementCom->Set_Transform(_transformCom);
        _movementCom->Sync_RuntimePositionToTransform(); // Reload/authoring 복원 뒤 CCT 위치를 Transform과 맞춘다.
        _movementCom->Stop_AllMovement();
        _movementCom->Clear_FollowMoveCommand();
        _movementCom->Apply_Input({});
    }

    if (_aiCtrl)
        _aiCtrl->Stop_Move();

    if (!data.contains("custom_properties") || !data["custom_properties"].is_object())
        return;

    const json& customProperties = data["custom_properties"];
    _battleStartSpawnerId = customProperties.value("battle_start_spawner_id", 0);
    _battleEncounterId = customProperties.value("battle_encounter_id", 1); // 새 스폰 포맷도 전투 필드 ID를 먼저 복원한다.
    _startIdleUntilPlayerInRange = customProperties.value("start_idle_until_player_in_range", false);
    _startIdleReleaseDistance = customProperties.value("start_idle_release_distance", 0.f);
    if (customProperties.contains("battle_start_offset"))
        (void)Json_ToVec3(customProperties["battle_start_offset"], _battleStartOffset);

    if (customProperties.contains("battle_monster_spawns") && customProperties["battle_monster_spawns"].is_array())
    {
        _battleMonsterSpawnDescs.clear();
        for (const json& spawnNode : customProperties["battle_monster_spawns"])
        {
            BattleMonsterSpawnDesc spawnDesc{};
            if (Json_ToBattleMonsterSpawnDesc(spawnNode, spawnDesc))
                _battleMonsterSpawnDescs.push_back(spawnDesc);
        }
        return;
    }

    if (!customProperties.contains("battle_monster_prefab_names") || !customProperties["battle_monster_prefab_names"].is_array())
        return;

    _battleMonsterSpawnDescs.clear();
    for (const json& prefabName : customProperties["battle_monster_prefab_names"])
    {
        if (prefabName.is_string())
        {
            BattleMonsterSpawnDesc spawnDesc{};
            spawnDesc.prefabName = prefabName.get<string>();
            _battleMonsterSpawnDescs.push_back(spawnDesc);
        }
    }
}

void Monster_WorldBase::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Character_WorldBase::OnBeginOverlap(self, other);

    if (_isBattleStart)
        return;

    // 이미 전투 중이거나, 충돌 후 2초 슬로우모션 진입 중이면
    if (CLIENT->Is_WorldControlSuspended() || CLIENT->Is_BattleEntering())
        return;

    const Shared<GameObject> overlapOwner = other ? other->Get_Owner() : nullptr;
    if (!overlapOwner)
        return;

    Shared<GameObject> heroOwner = overlapOwner;

    // 충돌된 플레이어가 Active 플레이어가 아니라면 배틀 진입 X
    // 월드액션 충돌체는 AN_SpawnActor가 owner를 플레이어로 심어주므로 한 단계 위 owner의 owner까지 가야함
    if (!CLIENT->Is_ActiveHero(heroOwner))
        heroOwner = overlapOwner->Get_Owner();

    if (!CLIENT->Is_ActiveHero(heroOwner))
        return;

    const bool isWorldActionTrigger =
        other->Get_CollisionPreset() == Collision_Preset::WorldActionTrigger;
    bool isPlayerAdvantage = isWorldActionTrigger;

    if (!isPlayerAdvantage)
    {
        const Shared<PlayerStateMachineCom> stateCom = heroOwner->Get_Component<PlayerStateMachineCom>();
        isPlayerAdvantage = stateCom && stateCom->Get_CurrentPlayerState() == PlayerState::WorldAction;
    }

    if (isPlayerAdvantage)
    {
        const Shared<TransformCom> transform = Get_Transform();
        if (transform)
        {
            PlayEffectDesc hitEffectDesc{};
            hitEffectDesc.effectName = kWorldActionHitEffect;
            hitEffectDesc.worldPosition = transform->Get_WorldPosition() + Vec3(0.f, 0.7f, 0.f);
            CLIENT->Play_Effect(hitEffectDesc);

            PlayEffectDesc distortionDesc{};
            distortionDesc.effectName = kWorldActionDistortionEffect;
            distortionDesc.worldPosition = transform->Get_WorldPosition() + Vec3(0.f, 0.6f, 0.f);
            CLIENT->Play_Effect(distortionDesc);
        }
    }

    _isBattleStart = true;

    if (self)
        self->Set_IsActive(false);

    // 전투 시작 플레이어 지정
    vector<HeroId> heroes = Build_BattleHeroIds();

    vector<BattleMonsterSpawnDesc> battleMonsterSpawnDescs{};
    Set_BattleMonsterSpawnDescs(battleMonsterSpawnDescs);

    CLIENT->Get_ScreenFxManager()->Play_BattleInCounterFx();

    if (battleMonsterSpawnDescs.empty())
    {
        LOG_ERROR("Battle monster prefab list is empty.");
        _isBattleStart = false;

        if (self)
            self->Set_IsActive(true);

        return;
    }

    GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_Encounter);

    // 이제, 턴 진입 조건에 따라서 플레이어, 몬스터 턴 관리
    CLIENT->OnEnterBattleField(Get_BattleFieldId(), isPlayerAdvantage, heroes, battleMonsterSpawnDescs, GetSharedPtr<GameObject>());
}

void Monster_WorldBase::On_EditorStopRuntime()
{
    Restore_AfterBattleExit();
}

void Monster_WorldBase::Set_BattleMonsterSpawnDescs(vector<BattleMonsterSpawnDesc>& outSpawnDescs) const
{
    outSpawnDescs = _battleMonsterSpawnDescs;

    const string sourceBattlePrefabName = Resolve_SourceBattlePrefabName(Get_SourcePrefabName());
    if (sourceBattlePrefabName.empty())
        return;

    if (!_combatStat)
        return;

    const BattleMonsterStatSnapshot statSnapshot = _combatStat->Create_BattleSyncSnapshot();

    for (BattleMonsterSpawnDesc& spawnDesc : outSpawnDescs)
    {
        if (spawnDesc.prefabName != sourceBattlePrefabName)
            continue;

        spawnDesc.statSnapshot = statSnapshot;
    }
}

vector<HeroId> Monster_WorldBase::Build_BattleHeroIds() const
{
    // BattleEncounterProfile이 있으면 조우 전용 파티를 쓰고, 없으면 기본 3인 파티로 돌아간다.
    return Resolve_BattleEncounterHeroIds(Get_BattleFieldId());
}

const vector<Vec3>& Monster_WorldBase::Get_EffectivePatrolPoints()
{
    if (!_PatrolPoints.empty())
        return _PatrolPoints;

    if (_defaultPatrolPoints.empty())
        Build_DefaultPatrolPoints();

    return _defaultPatrolPoints;
}

void Monster_WorldBase::Build_DefaultPatrolPoints()
{
    _defaultPatrolPoints.clear();

    if (_transformCom == nullptr)
        return;

    const Vec3 origin = _transformCom->Get_Position();
    static const Vec3 offsets[] =
    {
        {  0.f, 0.f,  0.f },
        {  3.f, 0.f, -2.f },
        {  5.f, 0.f,  0.f },
        {  3.f, 0.f,  2.f },
        {  0.f, 0.f,  0.f },
        { -3.f, 0.f, -2.f },
        { -5.f, 0.f,  0.f },
        { -3.f, 0.f,  2.f },
        {  0.f, 0.f,  0.f },
    };

    _defaultPatrolPoints.reserve(_countof(offsets));
    for (const Vec3& offset : offsets)
        _defaultPatrolPoints.push_back(origin + offset);
}

void Monster_WorldBase::Restore_AfterBattleExit()
{
    _isBattleStart = false; // 다시 월드 진입 가능하게
    Set_Visible(true);

    // 약점,
    if (auto* weakPoint = dynamic_cast<Mon_WeakPoint*>(Get_PartObject(L"Part_Weak")))
        weakPoint->Restore_WorldWeakPointAfterBattle();

    // 충돌체도 같이 키기
    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(true);
        _colliderCom->Clear_Overlap();   
    }
}

HRESULT Monster_WorldBase::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_AIController"), _aiCtrl), E_FAIL);
    _aiCtrl->Set_Owner(static_pointer_cast<Monster_WorldBase>(shared_from_this()));

    MovementCom::MoveDesc moveDesc{};
    moveDesc.walkSpeed = 2.f;
    moveDesc.runSpeed = 2.f;
    moveDesc.sprintSpeed = 2.f;
    moveDesc.turnSpeedDegree = 540.f;
    moveDesc.turnResponse = 10.f;
    moveDesc.jumpVelocity = 0.f;
    moveDesc.gravity = -40.f;
    moveDesc.backend = MovementCom::MoveBackend::PhysXCCT;
    moveDesc.sweep.skinWidth = 0.03f;
    moveDesc.sweep.groundSnapUp = 0.35f;
    moveDesc.sweep.groundSnapDown = 0.65f;
    moveDesc.sweep.walkableNormalY = 0.65f;
    moveDesc.sweep.groundY = 0.f;
    moveDesc.cct.radius = 0.32f;
    moveDesc.cct.height = 1.50f;
    moveDesc.cct.stepOffset = 0.45f;
    moveDesc.cct.contactOffset = 0.05f;
    moveDesc.cct.slopeLimitDeg = 55.f;
    moveDesc.cct.groundLerpSpeed = 15.f;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Movement"), _movementCom, &moveDesc), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_MonsterStat"), _combatStat), E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.6f, 0.f);
    obbDesc.extents = Vec3(0.5f, 0.6f, 0.5f);

    CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &obbDesc, Collision_Preset::Monster_WorldBody), E_FAIL);

    return S_OK;
}

void Monster_WorldBase::Free()
{
    if (_editorStopHandle.IsValid())
    {
        GAME->Get_DelegateHub().OnEditorStopRuntime.Remove(_editorStopHandle);
        _editorStopHandle.Reset();
    }

    Character_WorldBase::Free();

}
