#include "NPC_Base.h"
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


NPC_Base::NPC_Base(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character_WorldBase(device, context)
{
}

NPC_Base::NPC_Base(const NPC_Base& prototype)
    : Character_WorldBase(prototype)
{
}

HRESULT NPC_Base::Initialize_Prototype()
{
    return Character_WorldBase::Initialize_Prototype();
}

HRESULT NPC_Base::Initialize(void* arg)
{
    CHECK_FAILED(Character_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);



    return S_OK;
}

void NPC_Base::BeginPlay()
{
    Character_WorldBase::BeginPlay();
 
    if (!_editorStopHandle.IsValid())
        _editorStopHandle = GAME->Get_DelegateHub().OnEditorStopRuntime.Add(this, &NPC_Base::On_EditorStopRuntime);

    if (auto* body = dynamic_cast<Monster_Part*>(Get_PartObject(L"Part_Body")))
    {
        if (Shared<ModelCom> model = body->Get_Model())
        {
            model->Refresh_AnimStateMappingFromOwnerContext();
        }
    }
}

void NPC_Base::Update(float timeDelta)
{
    if (CLIENT->Is_WorldControlSuspended())
        return;

    auto* bodyPart = dynamic_cast<Monster_Part*>(Get_PartObject(L"Part_Body"));
    if (_isCinematicDriven)
    {
        if (bodyPart)
        {
            bodyPart->Play_Animation(timeDelta);
            _transformCom->Set_SpeedPerSec(0.f);
        }

        Character_WorldBase::Update(timeDelta);

        return;
    }

    const Shared<GameObject> player = CLIENT->Find_Player(GAME->Current_LevelIndex());

    Character_WorldBase::Update(timeDelta);
}


void NPC_Base::Late_Update(float timeDelta)
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

HRESULT NPC_Base::Render()
{
    return Character_WorldBase::Render();
}

HRESULT NPC_Base::Render_Shadow()
{
    return Character_WorldBase::Render_Shadow();
}

json NPC_Base::To_Json() const
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

void NPC_Base::From_Json(const json& data)
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

void NPC_Base::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
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

void NPC_Base::On_EditorStopRuntime()
{
    Restore_AfterBattleExit();
}

void NPC_Base::Set_BattleMonsterSpawnDescs(vector<BattleMonsterSpawnDesc>& outSpawnDescs) const
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

vector<HeroId> NPC_Base::Build_BattleHeroIds() const
{
    // BattleEncounterProfile이 있으면 조우 전용 파티를 쓰고, 없으면 기본 3인 파티로 돌아간다.
    return Resolve_BattleEncounterHeroIds(Get_BattleFieldId());
}

const vector<Vec3>& NPC_Base::Get_EffectivePatrolPoints()
{
    if (!_PatrolPoints.empty())
        return _PatrolPoints;

    if (_defaultPatrolPoints.empty())
        Build_DefaultPatrolPoints();

    return _defaultPatrolPoints;
}

void NPC_Base::Build_DefaultPatrolPoints()
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

void NPC_Base::Restore_AfterBattleExit()
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

HRESULT NPC_Base::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_AIController"), _aiCtrl), E_FAIL);
    _aiCtrl->Set_Owner(static_pointer_cast<NPC_Base>(shared_from_this()));

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

void NPC_Base::Free()
{
    if (_editorStopHandle.IsValid())
    {
        GAME->Get_DelegateHub().OnEditorStopRuntime.Remove(_editorStopHandle);
        _editorStopHandle.Reset();
    }

    Character_WorldBase::Free();

}
