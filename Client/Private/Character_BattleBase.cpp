#include "Character_BattleBase.h"
#include "PartObject.h"
#include "BattleController.h"
#include "ClientInstance.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "Monster_Part.h"
#include "MonsterStatCom.h"

Character_BattleBase::Character_BattleBase(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character(device, context)
{
}

Character_BattleBase::Character_BattleBase(const Character_BattleBase& prototype)
    : Character(prototype)
{
}

HRESULT Character_BattleBase::Initialize_Prototype()
{
    return Character::Initialize_Prototype();
}

HRESULT Character_BattleBase::Initialize(void* arg)
{
    return Character::Initialize(arg);
}

void Character_BattleBase::BeginPlay()
{
    Character::BeginPlay();

    // 첫 프레임에 미리 등록
    Update_Collider();
}

void Character_BattleBase::Late_Update(float timeDelta)
{
    Character::Late_Update(timeDelta);

    Update_Collider();
}

void Character_BattleBase::Adjust_IncomingBattleDamage(const BattleChar& attacker, float& damage,
    AttackType attackType) const
{
    UNREFERENCED_PARAMETER(attacker);
    UNREFERENCED_PARAMETER(damage);
    UNREFERENCED_PARAMETER(attackType);
}

bool Character_BattleBase::Try_ApplyBattleBreakOnDamage(const DamageAppliedInfo& info, bool canTriggerBreak)
{
    if (info.damage <= 0.f)
        return false;

    if (info.target.actor.get() != this)
        return false;

    const Shared<MonsterStatCom> monsterStat =
        dynamic_pointer_cast<MonsterStatCom>(info.target.statCom);

    if (monsterStat == nullptr || !monsterStat->Is_Boss())
        return false;

    if (monsterStat->Get_AliveStatus() == EAliveStatus::Dead)
        return false;

    if (monsterStat->IsBreak() || monsterStat->Get_MaxBreak() <= 0.f)
        return false;

    const bool wasBreakGaugeFullBeforeDamage =
        monsterStat->Get_CurBreak() >= monsterStat->Get_MaxBreak();

    monsterStat->Add_Break(info.damage);

    if (!wasBreakGaugeFullBeforeDamage)
        return false;

    if (!canTriggerBreak)
        return false;

    monsterStat->SetBreak(true);
    monsterStat->Set_Break(monsterStat->Get_MaxBreak());

    if (const auto battle = CLIENT->Get_BattleController())
        battle->Apply_Buff(info.target, Buffs::Stun, 1, 1);

    if (info.target.ModelCom != nullptr)
    {
        const bool playedBreakEnter =
            info.target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Stun_Enter), 0.2f, false);

        if (!playedBreakEnter)
            info.target.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Idle_Stun), 0.2f, true);
    }

    return true;
}

void Character_BattleBase::Set_BattleColliderActive(bool isActive)
{
    if (_colliderCom == nullptr)
        return;

    _colliderCom->Set_IsActive(isActive);

    if (!isActive)
        _colliderCom->Clear_Overlap();
}

bool Character_BattleBase::Is_BattleColliderActive() const
{
    if (_colliderCom == nullptr)
        return false;

    return _colliderCom->Get_IsActive();
}

bool Character_BattleBase::Try_GetDamageFeedbackWorldPosition(Vec3& outWorldPos) const
{
    if (Try_GetDamageFeedbackSocketWorldPosition(outWorldPos))
        return true;

    if (PartObject* body = Get_PartObject(L"Part_Body"))
    {
        if (auto* monsterBody = dynamic_cast<Monster_Part*>(body))
        {
            if (Shared<ModelCom> model = monsterBody->Get_Model())
            {
                Vec3 localMin{};
                Vec3 localMax{};
                if (model->Try_Get_LocalBounds(localMin, localMax))
                {
                    const Vec3 localCenter = (localMin + localMax) * 0.5f;
                    const float localHeight = max(localMax.y - localMin.y, 0.f);

                    Vec3 localAnchor = localCenter;
                    localAnchor.y = localMin.y + localHeight * 0.56f; // 데미지 숫자가 너무 위로 튀지 않게 몬스터 몸통 중앙 근처에 잡는다.

                    outWorldPos = Vec3::Transform(localAnchor, body->Get_CombinedWorldMatrix());
                    return true;
                }
            }
        }

        const Matrix& bodyWorld = body->Get_CombinedWorldMatrix();
        outWorldPos = Vec3(bodyWorld._41, bodyWorld._42, bodyWorld._43);
        outWorldPos += Get_DamageFeedbackFallbackOffset();
        return true;
    }

    if (_transformCom == nullptr)
        return false;

    outWorldPos = _transformCom->Get_WorldPosition();
    outWorldPos += Get_DamageFeedbackFallbackOffset();
    return true;
}

bool Character_BattleBase::Try_GetDamageFeedbackSocketWorldPosition(Vec3& outWorldPos) const
{
    return false;
}

Vec3 Character_BattleBase::Get_DamageFeedbackFallbackOffset() const
{
    return Vec3(0.f, 1.25f, 0.f);
}

HRESULT Character_BattleBase::Ready_Collider(const wstring& prototypeTag, void* colliderDesc, Collision_Preset preset)
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), prototypeTag, _colliderCom, colliderDesc), E_FAIL);

    // 충돌 preset은 하위 클래스가 명시 -> Player_Body, Monster_Body
    _colliderCom->Set_CollisionPreset(preset);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

void Character_BattleBase::Update_Collider()
{
    if (_colliderCom == nullptr)
        return;

    if (!_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

void Character_BattleBase::Free()
{
    Character::Free();
}
