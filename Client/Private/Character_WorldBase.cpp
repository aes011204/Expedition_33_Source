#include "Character_WorldBase.h"

#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "GameInstance.h"

Character_WorldBase::Character_WorldBase(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character(device, context)
{
}

Character_WorldBase::Character_WorldBase(const Character_WorldBase& prototype)
    : Character(prototype)
{
}

HRESULT Character_WorldBase::Initialize_Prototype()
{
    return Character::Initialize_Prototype();
}

HRESULT Character_WorldBase::Initialize(void* arg)
{
    return Character::Initialize(arg);
}

void Character_WorldBase::Late_Update(float timeDelta)
{
    Character::Late_Update(timeDelta);

    Update_Collider();
}

HRESULT Character_WorldBase::Ready_Collider(const wstring& prototypeTag, void* colliderDesc, Collision_Preset preset)
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), prototypeTag, _colliderCom, colliderDesc), E_FAIL);

    // 프리셋은 하위 클래스가 명시 -> Player_Body, Monster_Body
    _colliderCom->Set_CollisionPreset(preset);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

void Character_WorldBase::Update_Collider()
{
    if (_colliderCom == nullptr)
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);
}

void Character_WorldBase::Update_Culling(const Vec3& extents)
{
    // 일단 월드 캐릭터만 culling 처리
    _cullingAABB.Center = _transformCom->Get_Position();
    _cullingAABB.Extents = extents;
    Set_Visible(GAME->IsVisible(_cullingAABB));
}

void Character_WorldBase::Free()
{
    Character::Free();
}
