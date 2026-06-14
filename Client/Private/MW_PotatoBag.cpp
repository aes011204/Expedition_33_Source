#include "MW_PotatoBag.h"
#include "ClientInstance.h"
#include "Mon_PotatoBag_Body.h"
#include "Mon_PotatoBag_Weapon.h"
#include "Mon_PotatoBag_Shield.h"
#include "Mon_WeakPoint.h"

IMPLEMENT_REFLECTION(MW_PotatoBag)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_PotatoBag";
    info.category = "Monster";

    return true;
}

MW_PotatoBag::MW_PotatoBag(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_WorldBase(device, context)
{
}

MW_PotatoBag::MW_PotatoBag(const MW_PotatoBag& prototype)
    : Monster_WorldBase(prototype)
{
}

HRESULT MW_PotatoBag::Initialize_Prototype()
{
    return Monster_WorldBase::Initialize_Prototype();
}

HRESULT MW_PotatoBag::Initialize(void* arg)
{
    CHECK_FAILED(Monster_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void MW_PotatoBag::Priority_Update(float timeDelta)
{
    Monster_WorldBase::Priority_Update(timeDelta);
}

void MW_PotatoBag::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Monster_WorldBase::Update(timeDelta);
}

HRESULT MW_PotatoBag::Render()
{
    return S_OK;
}

HRESULT MW_PotatoBag::Render_Shadow()
{
    return S_OK;
}

void MW_PotatoBag::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_WorldBase::OnBeginOverlap(self, other);
}

HRESULT MW_PotatoBag::Ready_Components()
{
    CHECK_FAILED(Monster_WorldBase::Ready_Components(), E_FAIL);
    return S_OK;
}

HRESULT MW_PotatoBag::Ready_PartObjects()
{
    Mon_PotatoBag_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_PotatoBag";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_PotatoBag_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_PotatoBag_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    Mon_PotatoBag_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = ModelType::NonAnim;
    weaponDesc.modelTag = L"Model_PotatoBag_Weapon";
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_R");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_PotatoBag_Weapon"), TEXT("Part_Weapon"), &weaponDesc), E_FAIL);

    _weapon = dynamic_pointer_cast<Mon_PotatoBag_Weapon>(_partObjects[TEXT("Part_Weapon")]);
    CHECK_NULL(_weapon, E_FAIL);

    Mon_PotatoBag_Shield::SHIELD_DESC shieldDesc{};
    shieldDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    shieldDesc.modelType = ModelType::NonAnim;
    shieldDesc.modelTag = L"Model_PotatoBag_Shield";
    shieldDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_L");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_PotatoBag_Shield"), TEXT("Part_Shield"), &shieldDesc), E_FAIL);

    _shield = dynamic_pointer_cast<Mon_PotatoBag_Shield>(_partObjects[TEXT("Part_Shield")]);
    CHECK_NULL(_shield, E_FAIL);

    Mon_WeakPoint::WEAK_DESC weakDesc{};
    weakDesc.parentMatrix = &_body->Get_CombinedWorldMatrix();
    weakDesc.allowMissingModel = true;
    weakDesc.socketName = "upperarm_l";
    weakDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("upperarm_l");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_MonsterWeak"), TEXT("Part_Weak"), &weakDesc), E_FAIL);

    _weak = dynamic_pointer_cast<Mon_WeakPoint>(_partObjects[TEXT("Part_Weak")]);
    CHECK_NULL(_weak, E_FAIL);

    return S_OK;
}

Shared<MW_PotatoBag> MW_PotatoBag::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MW_PotatoBag>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_PotatoBag");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MW_PotatoBag::Clone(void* arg)
{
    auto clone = make_shared<MW_PotatoBag>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MW_PotatoBag::Free()
{
    Monster_WorldBase::Free();
}
