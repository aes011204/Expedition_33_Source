#include "MW_Simon.h"
#include "ClientInstance.h"
#include "Mon_Simon_Body.h"
#include "Mon_Simon_Weapon.h"

IMPLEMENT_REFLECTION(MW_Simon)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_Simon";
    info.category = "Monster";

    return true;
}


MW_Simon::MW_Simon(const ComPtr<Device>& device, const ComPtr<Context>& context) :Monster_WorldBase(device, context)
{
    _worldMoveSpeedScale = 0.1f;
}

MW_Simon::MW_Simon(const MW_Simon& prototype) : Monster_WorldBase(prototype)
{
}

HRESULT MW_Simon::Initialize_Prototype()
{
    return Monster_WorldBase::Initialize_Prototype();
}

HRESULT MW_Simon::Initialize(void* arg)
{
    CHECK_FAILED(Monster_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void MW_Simon::Priority_Update(float timeDelta)
{
    Monster_WorldBase::Priority_Update(timeDelta);
}

void MW_Simon::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Monster_WorldBase::Update(timeDelta);
}

HRESULT MW_Simon::Render()
{
    return S_OK;
}

HRESULT MW_Simon::Render_Shadow()
{
    return S_OK;
}

void MW_Simon::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_WorldBase::OnBeginOverlap(self, other);
}

HRESULT MW_Simon::Ready_Components()
{
    Monster_WorldBase::Ready_Components();
    return S_OK;
}

HRESULT MW_Simon::Ready_PartObjects()
{
    Mon_Simon_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Simon";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Simon_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Simon_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);


    Mon_Simon_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = ModelType::NonAnim;
    weaponDesc.modelTag = L"Model_Simon_Weapon";
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_R");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Simon_Weapon"), TEXT("Part_Weapon"), &weaponDesc), E_FAIL); // 배틀 몬스터 무기 파츠도 Static prototype을 재사용한다.

    _weapon = dynamic_pointer_cast<Mon_Simon_Weapon>(_partObjects[TEXT("Part_Weapon")]);
    CHECK_NULL(_weapon, E_FAIL);



    return S_OK;
}

Shared<MW_Simon> MW_Simon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MW_Simon>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Simon");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MW_Simon::Clone(void* arg)
{
    auto clone = make_shared<MW_Simon>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MW_Simon::Free()
{
    Monster_WorldBase::Free();
}
