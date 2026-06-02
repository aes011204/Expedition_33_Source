#include "Character_BattleBase.h"

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

void Character_BattleBase::Late_Update(float timeDelta)
{
    Character::Late_Update(timeDelta);

    Update_Collider();
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
