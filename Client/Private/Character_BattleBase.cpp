#include "Character_BattleBase.h"
#include "PartObject.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "GameInstance.h"

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

bool Character_BattleBase::Try_ApplyBattleBreakOnDamage(const DamageAppliedInfo& info)
{
    UNREFERENCED_PARAMETER(info);

    return false;
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
