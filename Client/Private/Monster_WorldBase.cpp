#include "Monster_WorldBase.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "MonsterStatCom.h"
#include "Bounding_OBB.h"
#include "Monster_Part.h"

REGISTER_GLOBAL_ENUM(Monster_WorldBase::MonsterWorldAnim, "MonsterWorldAnim")

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
    if (CLIENT->Is_BattleActive())
        return;

    if (_aiCtrl)
    {
        const Shared<BlackBoard> bb = _aiCtrl->Get_BlackBoard();
        const Shared<GameObject> player = CLIENT->Find_Player(GAME->Current_LevelIndex());

        if (bb && player)
        {
            bb->Set_Value("playerPos", player->Get_Transform()->Get_Position());
        }

        _aiCtrl->Update(timeDelta);
    }

    if (_combatStat)
        _combatStat->Update(timeDelta);


    //if (_aiCtrl && _aiCtrl->Get_IsMoving())
    //    Set_CurrentAction(MonsterAction::Move);


    if (auto* bodyPart = dynamic_cast<Monster_Part*>(Get_PartObject(L"Part_Body")))
    {
        if (_currentAction != _previousAction)
        {
            switch (_currentAction)
            {
            case MonsterWorldAnim::Idle:
                bodyPart->Set_AnimationByState(ETOI(MonsterWorldAnim::Idle), 0.3f, true);
                break;

            case MonsterWorldAnim::Patrol:
                bodyPart->Set_AnimationByState(ETOI(MonsterWorldAnim::Patrol), 0.3f, true);
                break;

            case MonsterWorldAnim::Chase:
                bodyPart->Set_AnimationByState(ETOI(MonsterWorldAnim::Chase), 0.3f, true);
                break;
            }

            _previousAction = _currentAction;
        }
    }


    Character_WorldBase::Update(timeDelta);
}

void Monster_WorldBase::Late_Update(float timeDelta)
{
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
    customProperties["battle_monster_prefab_names"] = _battleMonsterPrefabNames;

    return data;
}

void Monster_WorldBase::From_Json(const json& data)
{
    Character_WorldBase::From_Json(data);

    _battleMonsterPrefabNames.clear();

    if (!data.contains("custom_properties") || !data["custom_properties"].is_object())
        return;

    const json& customProperties = data["custom_properties"];
    if (!customProperties.contains("battle_monster_prefab_names") || !customProperties["battle_monster_prefab_names"].is_array())
        return;

    for (const json& prefabName : customProperties["battle_monster_prefab_names"])
    {
        if (prefabName.is_string())
            _battleMonsterPrefabNames.push_back(prefabName.get<string>());
    }
}

void Monster_WorldBase::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Character_WorldBase::OnBeginOverlap(self, other);

    if (_isBattleStart)
        return;

    // 이미 전투 중이거나, 충돌 후 2초 슬로우모션 진입 중이면
    if (CLIENT->Is_BattleActive() || CLIENT->Is_BattleEntering())
        return;

        const Shared<GameObject> overlapOwner = other ? other->Get_Owner() : nullptr;

    // 충돌된 플레이어가 Active 플레이어가 아니라면 배틀 진입 X
    if (!CLIENT->Is_ActiveHero(overlapOwner))
        return;

    _isBattleStart = true;

    if (self)
        self->Set_IsActive(false);

    // Player는 3명 고정 스폰, 몬스터는 자식 객체에서 Set_BattleMonster 함수로 지정
    vector<HeroId> heroes =
    {
        HeroId::Gustave,
        HeroId::Lune,
        HeroId::Maelle,
    };

    vector<string> battleMonsterPrefabNames;
    Set_BattleMonsterPrefabNames(battleMonsterPrefabNames);

    if (battleMonsterPrefabNames.empty())
    {
        LOG_ERROR("Battle monster prefab list is empty.");
        _isBattleStart = false;

        if (self)
            self->Set_IsActive(true);

        return;
    }

    CLIENT->OnEnterBattleField(1, true, heroes, battleMonsterPrefabNames, GetSharedPtr<GameObject>());
}

void Monster_WorldBase::On_EditorStopRuntime()
{
    _isBattleStart = false;

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(true);
        _colliderCom->Clear_Overlap();
    }
}

void Monster_WorldBase::Set_BattleMonsterPrefabNames(vector<string>& outPrefabNames) const
{
    outPrefabNames = _battleMonsterPrefabNames;
}

HRESULT Monster_WorldBase::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_AIController"), _aiCtrl), E_FAIL);
    _aiCtrl->Set_Owner(static_pointer_cast<Monster_WorldBase>(shared_from_this()));

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_MonsterStat"), _combatStat), E_FAIL);

    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.5f, 0.f);
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
