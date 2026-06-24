#include "SL_EstherManager.h"
#include "Body_Player.h"
#include "ClientInstance.h"
#include "ModelCom.h"
#include "Player_BattleBase.h"
#include "SL_Base.h"
#include "SL_DamageSystem.h"
#include "SL_Maelle.h"
#include "SoulLikeController.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"

namespace
{
    constexpr float kEstherBaseDamage = 100.f;
    constexpr float kEstherGaugeMax = 100.f;
    constexpr float kEstherParryGaugeGain = 20.f;

    constexpr char kGustaveSkill[] = "UnleashedCharge";
    constexpr char kLuneSkill[] = "Immolation";

    //constexpr char kGustaveAnim[] = "Gustave_Noah_Skill_Overcharge_VeryStrong";
    constexpr char kGustaveAnim[] = "Gustave_Verso_Skill_Flawless_Spiral";
    constexpr char kLuneAnim[] = "Lune_Unstable_Fire";
}

HRESULT SL_EstherManager::Initialize(const Shared<SoulLikeController>& controller)
{
    CHECK_NULL(controller, E_FAIL);

    _controller = controller;
    _activeEsther.reset();
    _estherGauge = 0.f;

    return S_OK;
}

void SL_EstherManager::Update(float timeDelta)
{
    Update_ActiveEsther(timeDelta);
}

void SL_EstherManager::Reset()
{
    Finish_ActiveEsther();
    _activeEsther.reset();
    _estherGauge = 0.f;
}

bool SL_EstherManager::Request_Esther(SL_EstherType estherType)
{
    if (_activeEsther.has_value())
        return false;

    if (!Is_GaugeFull())
        return false;

    if (!Begin_Esther(estherType))
        return false;

    _estherGauge = 0.f;
    return true;
}

bool SL_EstherManager::Notify_Attack(GameObject* notifyOwner)
{
    if (!_activeEsther.has_value())
        return false;

    if (!Is_NotifyOwnerActiveEsther(notifyOwner))
        return false;

    EstherRuntime& runtime = *_activeEsther;

    if (!runtime.attackApplied)
    {
        Apply_EstherDamage(runtime);
        runtime.attackApplied = true;
    }

    return true;
}

void SL_EstherManager::Add_GaugeFromParry()
{
    _estherGauge = clamp(_estherGauge + kEstherParryGaugeGain, 0.f, kEstherGaugeMax);
}

float SL_EstherManager::Get_GaugeRatio() const
{
    return clamp(_estherGauge / max(kEstherGaugeMax, 1.f), 0.f, 1.f);
}

bool SL_EstherManager::Is_GaugeFull() const
{
    return _estherGauge >= kEstherGaugeMax;
}

bool SL_EstherManager::Begin_Esther(SL_EstherType estherType)
{
    Shared<SL_Base> target = Resolve_Target();
    if (!target || target->Is_Dead())
        return false;

    auto actor = Spawn_EstherActor(estherType, target);
    CHECK_NULL(actor, false);

    EstherRuntime runtime{};
    runtime.type = estherType;
    runtime.actor = actor;
    runtime.target = target;
    runtime.levelIndex = GAME->Current_LevelIndex();

    switch (estherType)
    {
    case SL_EstherType::GustaveOvercharge:
        runtime.skillKey = kGustaveSkill;
        runtime.animationName = kGustaveAnim;
        runtime.fallbackDamageMultiplier = 2.f;
        runtime.animationSpeed = 2.f;
        break;
    case SL_EstherType::LuneImmolation:
        runtime.skillKey = kLuneSkill;
        runtime.animationName = kLuneAnim;
        runtime.fallbackDamageMultiplier = 1.2f;
        runtime.animationSpeed = 2.f;
        break;
    }

    if (!Play_EstherAnimation(actor, runtime.animationName, runtime.animationSpeed))
    {
        GAME->Remove_GameObject(runtime.levelIndex, actor);
        return false;
    }

    _activeEsther = runtime;

    return true;
}

void SL_EstherManager::Update_ActiveEsther(float timeDelta)
{
    if (!_activeEsther.has_value())
        return;

    EstherRuntime& runtime = *_activeEsther;

    auto actor = runtime.actor.lock();
    if (!actor)
    {
        _activeEsther.reset();
        return;
    }

    Body_Player* body = dynamic_cast<Body_Player*>(actor->Get_PartObject(L"Part_Body"));
    Shared<ModelCom> model = body ? body->Get_Model() : nullptr;

    if (model != nullptr && !model->Is_AnimationFinished())
        return;

    Finish_ActiveEsther();
    _activeEsther.reset();
}

void SL_EstherManager::Finish_ActiveEsther()
{
    if (!_activeEsther.has_value())
        return;

    if (Shared<Player_BattleBase> actor = _activeEsther->actor.lock())
        GAME->Remove_GameObject(_activeEsther->levelIndex, actor);
}

Shared<SL_Base> SL_EstherManager::Resolve_Target() const
{
    Shared<SoulLikeController> controller = _controller.lock();

    return controller ? controller->Get_Monster() : nullptr;
}

Shared<Player_BattleBase> SL_EstherManager::Spawn_EstherActor(SL_EstherType estherType, const Shared<SL_Base>& target)
{
    Shared<SoulLikeController> controller = _controller.lock();
    Shared<SL_Maelle> player = controller ? controller->Get_Player() : nullptr;
    if (player == nullptr || target == nullptr)
        return nullptr;

    const char* prefabName = "";
    switch (estherType)
    {
    case SL_EstherType::GustaveOvercharge:
        prefabName = "PB_Gustave";
        break;
    case SL_EstherType::LuneImmolation:
        prefabName = "PB_Lune";
        break;
    }

    const Vec3 spawnPosition = Resolve_SpawnPosition(player, target);
    const Vec3 spawnRotation = Resolve_SpawnRotation(spawnPosition, target);

    Shared<GameObject> object = Spawn_Prefab::Prefab(prefabName)
        .AtLevel(GAME->Current_LevelIndex())
        .InLayer(TEXT("Layer_Player"))
        .Position(spawnPosition)
        .Rotation(spawnRotation)
        .Spawn();

    return dynamic_pointer_cast<Player_BattleBase>(object);
}

bool SL_EstherManager::Play_EstherAnimation(const Shared<Player_BattleBase>& actor, const string& animationName, float animationSpeed)
{
    if (actor == nullptr)
        return false;

    Body_Player* body = dynamic_cast<Body_Player*>(actor->Get_PartObject(L"Part_Body"));
    Shared<ModelCom> model = body ? body->Get_Model() : nullptr;
    if (model == nullptr)
        return false;

    if (model->Get_AnimationIndexByName(animationName) == UINT_MAX)
        return false;

    model->Set_AnimationSpeedByName(animationName, max(0.01f, animationSpeed));
    model->Set_Animation(animationName, 0.05f, false);

    return true;
}

bool SL_EstherManager::Is_NotifyOwnerActiveEsther(GameObject* notifyOwner) const
{
    if (!_activeEsther.has_value() || notifyOwner == nullptr)
        return false;

    Shared<Player_BattleBase> actor = _activeEsther->actor.lock();
    if (actor == nullptr)
        return false;

    Shared<GameObject> current = notifyOwner->GetSharedPtr<GameObject>();
    while (current != nullptr)
    {
        if (current.get() == actor.get())
            return true;

        current = current->Get_Owner();
    }

    return false;
}

void SL_EstherManager::Apply_EstherDamage(const EstherRuntime& runtime)
{
    Shared<SL_Base> target = runtime.target.lock();
    if (target == nullptr || target->Is_Dead())
        return;

    const Skill_Data* skillData = CLIENT->Get_Skill(runtime.skillKey);
    const float multiplier = skillData ? skillData->powerMultiplier : runtime.fallbackDamageMultiplier;

    SL_DamageInfo damageInfo{};
    if (!SL_DamageSystem::Apply_FixedDamage(
        runtime.actor.lock(),
        target,
        target->Get_Stat(),
        kEstherBaseDamage * multiplier,
        AttackType::SingleSkill,
        damageInfo))
    {
        return;
    }

    target->Apply_Damage(damageInfo);
}

Vec3 SL_EstherManager::Resolve_SpawnPosition(const Shared<SL_Maelle>& player, const Shared<SL_Base>& target) const
{
    const Shared<TransformCom> playerTr = player->Get_Transform();
    const Shared<TransformCom> targetTr = target->Get_Transform();

    if (!playerTr || !targetTr)
        return Vec3::Zero;;

    const Vec3 targetPos = targetTr->Get_WorldPosition();
    Vec3 fromTargetToPlayer = playerTr->Get_WorldPosition() - targetPos;

    fromTargetToPlayer.y = 0.f;
    if (fromTargetToPlayer.LengthSquared() <= 0.0001f)
        fromTargetToPlayer = Vec3::Look;
    else
        fromTargetToPlayer.Normalize();

    // 일단 임시 위치. 나중에 좌, 우 적당히 줄듯 스킬 연출에 따라서
    return targetPos + fromTargetToPlayer * 1.8f;
}

Vec3 SL_EstherManager::Resolve_SpawnRotation(const Vec3& spawnPosition, const Shared<SL_Base>& target) const
{
    const auto targetTr = target->Get_Transform();
    const Vec3 targetPos = targetTr->Get_WorldPosition();

    Vec3 lookDir = targetPos - spawnPosition;
    lookDir.y = 0.f;

    if (lookDir.LengthSquared() <= 0.0001f)
        return Vec3::Zero;

    lookDir.Normalize();

    const float yaw = XMConvertToDegrees(atan2f(lookDir.x, lookDir.z));

    return Vec3(0.f, yaw, 0.f);
}

Shared<SL_EstherManager> SL_EstherManager::Create(const Shared<SoulLikeController>& owner)
{
    Shared<SL_EstherManager> instance = make_shared<SL_EstherManager>();

    if (FAILED(instance->Initialize(owner)))
        return nullptr;

    return instance;
}

void SL_EstherManager::Free()
{
    Reset();

    _controller.reset();

    Base::Free();
}
