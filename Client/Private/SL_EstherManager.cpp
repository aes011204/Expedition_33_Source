#include "SL_EstherManager.h"
#include "Body_Player.h"
#include "ClientInstance.h"
#include "EffectCom.h"
#include "ModelCom.h"
#include "MonsterStatCom.h"
#include "Player_BattleBase.h"
#include "SL_Base.h"
#include "SL_DamageSystem.h"
#include "SL_Dualliste.h"
#include "SL_Maelle.h"
#include "SoulLikeController.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"
#include "MovementCom.h"

namespace
{
    constexpr float kEstherMaxHpDamageRatio = 0.1f;
    constexpr float kEstherGaugeMax = 100.f;

    constexpr float kEstherParryGaugeGain = 10.f;
    constexpr float kEstherGaugeFillEaseSpeed = 8.f;
    constexpr float kEstherTargetAnimSlowScale = 0.08f;
    constexpr uint32 kGustaveAttackCount = 8;
    constexpr uint32 kLuneAttackCount = 1;

    constexpr char kGustaveSkill[] = "UnleashedCharge";
    constexpr char kLuneSkill[] = "Immolation";

    //constexpr char kGustaveAnim[] = "Gustave_Noah_Skill_Overcharge_VeryStrong";
    constexpr char kGustaveAnim[] = "SL_Gustave_Verso_Skill_Flawless_Spiral"; // 천사의 눈
    //constexpr char kLuneAnim[] = "Lune_Unstable_Fire";
    constexpr char kLuneAnim[] = "SL_Lune_Fountain"; // 하늘파괴

    struct TargetEffectDesc
    {
        wstring effectName{};
        Vec3 localOffset{ Vec3::Zero };
        Vec3 localRotation{ Vec3::Zero };
        Vec3 scale{ Vec3::One };
    };

    const TargetEffectDesc kGustaveAngelsEyeSlashEffects[] =
    {
        { L"Gustave_AngelsEye_Slash", Vec3(-0.5f, 0.7f, -0.8f), Vec3(0.f, 0.f, -30.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(0.5f, 0.7f, -1.2f), Vec3(0.f, 0.f, 50.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(-0.4f, 0.7f, -1.f), Vec3(0.f, 0.f, -20.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(0.3f, 0.7f, -0.6f), Vec3(0.f, 0.f, -7.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(0.f, 0.7f, -0.6f), Vec3(0.f, 0.f, -75.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(0.f, 0.7f, -0.9f), Vec3(0.f, 0.f, 20.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(-1.f, 0.7f, -0.7f), Vec3(0.f, 0.f, 90.f), Vec3::One },
        { L"Gustave_AngelsEye_Slash", Vec3(0.9f, 0.7f, -0.8f), Vec3(0.f, 0.f, -90.f), Vec3::One },
    };
}

HRESULT SL_EstherManager::Initialize(const Shared<SoulLikeController>& controller)
{
    CHECK_NULL(controller, E_FAIL);

    _controller = controller;
    _activeEsther.reset();
    _estherGauge = 0.f;
    _displayEstherGauge = 0.f;

    return S_OK;
}

void SL_EstherManager::Update(float timeDelta)
{
    const float targetGauge = _estherGauge;
    if (_displayEstherGauge > targetGauge || timeDelta <= 0.f)
    {
        _displayEstherGauge = targetGauge;
    }
    else if (_displayEstherGauge < targetGauge)
    {
        const float rawAlpha = clamp(timeDelta * kEstherGaugeFillEaseSpeed, 0.f, 1.f);
        const float inverseAlpha = 1.f - rawAlpha;
        const float easedAlpha = 1.f - (inverseAlpha * inverseAlpha * inverseAlpha);
        _displayEstherGauge = std::lerp(_displayEstherGauge, targetGauge, easedAlpha);

        if (abs(_displayEstherGauge - targetGauge) <= 0.01f)
            _displayEstherGauge = targetGauge;
    }

    Update_ActiveEsther(timeDelta);
}

void SL_EstherManager::Reset()
{
    Finish_ActiveEsther();
    _activeEsther.reset();
    _estherGauge = 0.f;
    _displayEstherGauge = 0.f;
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
    _displayEstherGauge = 0.f;
    return true;
}

bool SL_EstherManager::Notify_Attack(GameObject* notifyOwner)
{
    if (!_activeEsther.has_value())
        return false;

    if (!Is_NotifyOwnerActiveEsther(notifyOwner))
        return false;

    EstherRuntime& runtime = *_activeEsther;

    const uint32 expectedAttackCount = runtime.expectedAttackCount > 0 ? runtime.expectedAttackCount : 1;
    if (runtime.appliedAttackCount >= expectedAttackCount)
        return true;

    const float damageRatio = kEstherMaxHpDamageRatio / static_cast<float>(expectedAttackCount);
    const bool isFinalAttack = runtime.appliedAttackCount + 1 >= expectedAttackCount;
    if (Apply_EstherDamage(runtime, damageRatio, isFinalAttack))
    {
        if (runtime.type == SL_EstherType::GustaveOvercharge)
        {
            Shared<SL_Base> target = runtime.target.lock();
            Shared<EffectCom> effectCom = target ? target->Get_Component<EffectCom>() : nullptr;

            if (effectCom != nullptr)
            {
                const uint32 slashEffectCount = static_cast<uint32>(size(kGustaveAngelsEyeSlashEffects));
                const uint32 slashEffectIndex = min(runtime.appliedAttackCount, slashEffectCount - 1);
                const TargetEffectDesc& slashEffect = kGustaveAngelsEyeSlashEffects[slashEffectIndex];

                EffectCom::EffectPlayDesc effectDesc{};
                effectDesc.levelIndex = runtime.levelIndex;
                effectDesc.effectName = slashEffect.effectName;
                effectDesc.target = EffectCom::EffectPlayTarget::OwnerRoot;
                effectDesc.localOffset = slashEffect.localOffset;
                effectDesc.localRotation = slashEffect.localRotation;
                effectDesc.scale = slashEffect.scale;
                effectDesc.followTarget = false;

                effectCom->Play_Effect(effectDesc);
            }
        }

        ++runtime.appliedAttackCount;

        if (runtime.appliedAttackCount >= expectedAttackCount)
            Restore_TargetSlow(runtime);
    }

    return true;
}

void SL_EstherManager::Add_GaugeFromParry()
{
    _estherGauge = clamp(_estherGauge + kEstherParryGaugeGain, 0.f, kEstherGaugeMax);
}

float SL_EstherManager::Get_GaugeRatio() const
{
    return clamp(_displayEstherGauge / max(kEstherGaugeMax, 1.f), 0.f, 1.f);
}

bool SL_EstherManager::Is_GaugeFull() const
{
    return _estherGauge >= kEstherGaugeMax;
}

void SL_EstherManager::Fill_GaugeForDebug()
{
    _estherGauge = kEstherGaugeMax;
    _displayEstherGauge = kEstherGaugeMax;
}

bool SL_EstherManager::Begin_Esther(SL_EstherType estherType)
{
    Shared<SL_Base> target = Resolve_Target();
    if (!target || target->Is_Dead())
        return false;

    const Shared<TransformCom> targetTransform = target->Get_Transform();
    if (targetTransform == nullptr)
        return false;

    const Vec3 targetAnchorWorldPosition = targetTransform->Get_WorldPosition();

    auto actor = Spawn_EstherActor(estherType, target);
    CHECK_NULL(actor, false);

    EstherRuntime runtime{};
    runtime.type = estherType;
    runtime.actor = actor;
    runtime.target = target;
    runtime.levelIndex = GAME->Current_LevelIndex();
    runtime.targetAnchorWorldPosition = targetAnchorWorldPosition;

    switch (estherType)
    {
    case SL_EstherType::GustaveOvercharge:
        runtime.skillKey = kGustaveSkill;
        runtime.animationName = kGustaveAnim;
        runtime.animationSpeed = 1.f;
        runtime.expectedAttackCount = kGustaveAttackCount;
        break;
    case SL_EstherType::LuneImmolation:
        runtime.skillKey = kLuneSkill;
        runtime.animationName = kLuneAnim;
        runtime.animationSpeed = 1.f;
        runtime.expectedAttackCount = kLuneAttackCount;
        break;
    }

    if (!Play_EstherAnimation(actor, runtime.animationName, runtime.animationSpeed))
    {
        GAME->Remove_GameObject(runtime.levelIndex, actor);
        return false;
    }

    Apply_TargetSlow(runtime);

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
        Restore_TargetSlow(runtime);
        _activeEsther.reset();
        return;
    }

    Body_Player* body = dynamic_cast<Body_Player*>(actor->Get_PartObject(L"Part_Body"));
    Shared<ModelCom> model = body ? body->Get_Model() : nullptr;

    if (model != nullptr && runtime.type == SL_EstherType::LuneImmolation)
    {
        const float currentTimeSec = model->Get_CurrentAnimationTimeSec();
        const float previousTimeSec = runtime.previousAnimationTimeSec;

        auto playAnchorEffect = [&](float triggerTimeSec, bool& played, const wchar_t* effectName, const Vec3& worldOffset, const Vec3& scale)
        {
            if (played)
                return;

            if (triggerTimeSec < previousTimeSec || triggerTimeSec > currentTimeSec)
                return;

            PlayEffectDesc effectDesc{};
            effectDesc.effectName = effectName;
            effectDesc.layerTag = L"Layer_Effect";
            effectDesc.worldPosition = runtime.targetAnchorWorldPosition + worldOffset;
            effectDesc.scale = scale;

            CLIENT->Play_Effect(effectDesc);
            played = true;
        };

        playAnchorEffect(
            0.8333333134651184f,
            runtime.luneChargeEffectPlayed,
            L"Lune_SkyBreak_Charge_v2",
            Vec3(0.f, 5.800000190734863f, 0.f),
            Vec3(1.f, 1.f, 1.f));
        playAnchorEffect(
            1.5666667222976685f,
            runtime.luneMeteorEffectPlayed,
            L"Lune_SkyBreak_Meteor",
            Vec3(0.f, 5.800000190734863f, 0.f),
            Vec3(1.2000000476837158f, 1.2000000476837158f, 1.2000000476837158f));
        playAnchorEffect(
            4.849999904632568f,
            runtime.luneBeamEffectPlayed,
            L"Lune_SkyBreak_Beam",
            Vec3::Zero,
            Vec3(1.f, 1.f, 1.f));
        playAnchorEffect(
            5.866666793823242f,
            runtime.luneFinalExplosionEffectPlayed,
            L"Lune_SkyBreak_FinalExplosion",
            Vec3::Zero,
            Vec3(1.100000023841858f, 1.100000023841858f, 1.100000023841858f));

        runtime.previousAnimationTimeSec = currentTimeSec;
    }

    if (model != nullptr && !model->Is_AnimationFinished())
        return;

    Finish_ActiveEsther();
    _activeEsther.reset();
}

void SL_EstherManager::Finish_ActiveEsther()
{
    if (!_activeEsther.has_value())
        return;

    Restore_TargetSlow(*_activeEsther);

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

    const Vec3 spawnPosition = Resolve_SpawnPosition(estherType, player, target);
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

bool SL_EstherManager::Apply_EstherDamage(const EstherRuntime& runtime, float damageRatio, bool applyBreak)
{
    Shared<SL_Base> target = runtime.target.lock();
    if (target == nullptr || target->Is_Dead())
        return false;

    Shared<CombatStatCom> targetStat = target->Get_Stat();
    if (targetStat == nullptr)
        return false;

    const float maxHp = max(targetStat->Get_StatInfo().maxHp, 0.f);
    const float fixedDamage = maxHp * max(0.f, damageRatio);

    SL_DamageInfo damageInfo{};
    if (!SL_DamageSystem::Apply_FixedDamage(
        runtime.actor.lock(),
        target,
        targetStat,
        fixedDamage,
        AttackType::SingleSkill,
        damageInfo))
    {
        return false;
    }

    if (applyBreak)
    {
        if (Shared<MonsterStatCom> monsterStat = dynamic_pointer_cast<MonsterStatCom>(targetStat))
            monsterStat->Set_Break(monsterStat->Get_MaxBreak());
    }

    target->Apply_Damage(damageInfo);
    return true;
}

void SL_EstherManager::Apply_TargetSlow(EstherRuntime& runtime)
{
    const Shared<SL_Dualliste> dualliste = dynamic_pointer_cast<SL_Dualliste>(runtime.target.lock());
    if (dualliste == nullptr || dualliste->Is_Dead())
        return;

    dualliste->Set_ActionLocked(true);
    dualliste->Set_EstherTimeScale(kEstherTargetAnimSlowScale);

    if (const Shared<MovementCom> movement = dualliste->Get_MovementCom())
    {
        movement->Clear_FollowMoveCommand();
        movement->Apply_Input({});
        movement->Stop_AllMovement();
    }

    runtime.targetSlowApplied = true;
}

void SL_EstherManager::Restore_TargetSlow(EstherRuntime& runtime)
{
    if (!runtime.targetSlowApplied)
        return;

    const Shared<SL_Dualliste> dualliste = dynamic_pointer_cast<SL_Dualliste>(runtime.target.lock());
    if (dualliste != nullptr)
    {
        dualliste->Set_ActionLocked(false);
        dualliste->Set_EstherTimeScale(1.f);
    }

    runtime.targetSlowApplied = false;
}

Vec3 SL_EstherManager::Resolve_SpawnPosition(SL_EstherType estherType, const Shared<SL_Maelle>& player,
    const Shared<SL_Base>& target) const
{
    const Shared<TransformCom> playerTr = player->Get_Transform();
    const Shared<TransformCom> targetTr = target->Get_Transform();

    if (!playerTr || !targetTr)
        return Vec3::Zero;

    const Vec3 targetPos = targetTr->Get_WorldPosition();

    Vec3 fromTargetToPlayer = playerTr->Get_WorldPosition() - targetPos;
    fromTargetToPlayer.y = 0.f;

    if (fromTargetToPlayer.LengthSquared() <= 0.0001f)
        fromTargetToPlayer = Vec3::Look;
    else
        fromTargetToPlayer.Normalize();

    if (estherType == SL_EstherType::LuneImmolation)
    {
        Vec3 right = Vec3::Up.Cross(fromTargetToPlayer);
        if (right.LengthSquared() <= 0.0001f)
            right = Vec3::Right;
        else
            right.Normalize();

        constexpr float kLuneForwardOffset = 3.0f;
        constexpr float kLuneRightOffset = -2.0f;
        constexpr float kLuneHeightOffset = 0.f;

        return targetPos
            + fromTargetToPlayer * kLuneForwardOffset
            + right * kLuneRightOffset
            + Vec3(0.f, kLuneHeightOffset, 0.f);
    }

    // 구스타브는 기존 위치 유지
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
