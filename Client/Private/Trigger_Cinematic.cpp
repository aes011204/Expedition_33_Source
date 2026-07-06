#include "Trigger_Cinematic.h"

#include "BattleController.h"
#include "Client_Constants.h"
#include "ColliderCom.h"
#include "Bounding_AABB.h"
#include "ClientInstance.h"
#include "Monster_WorldBase.h"
#include "RuntimeRuleStateService.h"

IMPLEMENT_REFLECTION(Trigger_Cinematic)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Trigger_Cinematic";
    info.category = "Trigger";

    PROPERTY_WSTRING("Sequence File Id", _sequenceFileId);

    return true;
}

Trigger_Cinematic::Trigger_Cinematic(const ComPtr<Device>& device, const ComPtr<Context>& context) : GameObject(device, context)
{
}

Trigger_Cinematic::Trigger_Cinematic(const Trigger_Cinematic& prototype) : GameObject(prototype)
{
}

HRESULT Trigger_Cinematic::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Trigger_Cinematic::Initialize(void* arg)
{
    GAMEOBJECT_DESC desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(__super::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void Trigger_Cinematic::Update(float timeDelta)
{
    GameObject::Update(timeDelta);
}

void Trigger_Cinematic::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);

    if (_colliderCom == nullptr || !_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

void Trigger_Cinematic::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnBeginOverlap(self, other);

    if (self)
    {
        self->Set_IsActive(false);
        self->Clear_Overlap();

        if (_onTriggered)
            _onTriggered();

		const wstring sequencePath = Constants::Cinematic::RuntimeJson_Dir + _sequenceFileId;
        const bool isBishopCinematic =
            _sequenceFileId == Constants::Cinematic::BishopCinematicFileId;
        const bool isSimonIntroCinematic =
            _sequenceFileId == Constants::Cinematic::SimonIntroFileId;

        if (isBishopCinematic)
        {
            if (const Shared<BattleController> battleController = CLIENT->Get_BattleController())
                battleController->Capture_PreBattleBGM();

            GAME->Play_BGM(Constants::SoundBGM::BishopBattleBossMain, 0.7f, true, 0.f);
        }

        if (isSimonIntroCinematic)
        {
            GAME->Play_BGM(Constants::SoundBGM::SimonIntroCinematic, 0.7f, true, 0.f);
        }

        CLIENT->Clear_CinematicCompletedCallbacks();

        if (_startBattleOnComplete)
        {
            const string battleTargetId = _battleTargetId;
            const bool applyBishopStartRuleState =
                _sequenceFileId == Constants::Cinematic::BishopCinematicFileId;

            CLIENT->Add_CinematicCompletedCallback(
                [battleTargetId, applyBishopStartRuleState]()
                {
                    const Shared<GameObject> boundActor = CLIENT->Get_CinematicBoundActor(battleTargetId);
                    const Shared<Monster_WorldBase> worldMonster = dynamic_pointer_cast<Monster_WorldBase>(boundActor);
                    if (worldMonster == nullptr)
                    {
                        LOG_WARN("[Trigger_Cinematic] Battle start skipped: cinematic target '{}' is not a world monster.", battleTargetId);
                        return;
                    }

                    if (applyBishopStartRuleState)
                    {
                        RuntimeRuleStateService::Apply_ToCurrentRuntime(
                            RuntimeRuleStateService::Build_BishopStartUnlocks(), false);
                        CLIENT->Restore_CurrentHeroesHpToMax();
                    }

                    worldMonster->Begin_BattleFromCinematic(false); // 시네마틱 종료 진입은 월드액션 선공이 아니며, 내부에서 즉시 전투로 들어간다.
                });
        }

        if (!CLIENT->Request_CinematicSequenceTransition(sequencePath, 0))
            CLIENT->Play_Sequence(sequencePath, 0);
    }
}

HRESULT Trigger_Cinematic::Ready_Components()
{
    Bounding_AABB::BoundingAABBDesc aabbDesc{};
    aabbDesc.center = Vec3(0.f, 0.7f, 0.8f);
    aabbDesc.extents = Vec3(0.45f, 0.45f, 0.9f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_AABB"), _colliderCom, &aabbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Trigger);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

void Trigger_Cinematic::Set_SequenceFileId(const wstring& sequenceFileId)
{
    _sequenceFileId = sequenceFileId;
}

void Trigger_Cinematic::Set_StartBattleOnComplete(bool enabled, const string& targetId)
{
    _startBattleOnComplete = enabled;
    _battleTargetId = targetId;
}

void Trigger_Cinematic::Set_OnTriggered(function<void()> callback)
{
    _onTriggered = callback;
}

Shared<Trigger_Cinematic> Trigger_Cinematic::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Trigger_Cinematic>(device, context);

    if(FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Trigger_Cinematic");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Trigger_Cinematic::Clone(void* arg)
{
    auto clone = make_shared<Trigger_Cinematic>(*this);

    if(FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : Trigger_Cinematic");
        return nullptr;
    }
    return clone;
}

void Trigger_Cinematic::Free()
{
    GameObject::Free();
}
