#include "SL_Dualliste.h"

#include "ClientInstance.h"
#include "Mon_Dualliste_Body.h"
#include "Mon_Dualliste_Sword_L.h"
#include "Mon_Dualliste_Sword_R.h"

IMPLEMENT_REFLECTION(SL_Dualliste)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_Dualliste";
    info.category = "Monster";

    return true;
}

SL_Dualliste::SL_Dualliste(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character_WorldBase(device, context)
{

}

SL_Dualliste::SL_Dualliste(const SL_Dualliste& prototype)
    : Character_WorldBase(prototype)
{
}

HRESULT SL_Dualliste::Initialize_Prototype()
{
    return Character_WorldBase::Initialize_Prototype();
}

HRESULT SL_Dualliste::Initialize(void* arg)
{
    CHECK_FAILED(Character_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void SL_Dualliste::BeginPlay()
{
    Character_WorldBase::BeginPlay();
}

void SL_Dualliste::Priority_Update(float timeDelta)
{
    Character_WorldBase::Priority_Update(timeDelta);
}

void SL_Dualliste::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Character_WorldBase::Update(timeDelta);
}

void SL_Dualliste::Late_Update(float timeDelta)
{
    Character_WorldBase::Late_Update(timeDelta);

    Update_Culling(Vec3(1.f, 2.f, 1.f));
}

HRESULT SL_Dualliste::Render()
{
    return S_OK;
}

HRESULT SL_Dualliste::Render_Shadow()
{
    return S_OK;
}

void SL_Dualliste::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Character_WorldBase::OnBeginOverlap(self, other);
}

HRESULT SL_Dualliste::Ready_Components()
{
    return S_OK;
}

HRESULT SL_Dualliste::Ready_PartObjects()
{
    Mon_Dualliste_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Dualliste";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Dualliste_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    Mon_Dualliste_Sword_L::SWORD_DESC swordLDesc{};
    swordLDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    swordLDesc.modelType = ModelType::NonAnim;
    swordLDesc.modelTag = L"Model_Dualliste_Sword_L";
    swordLDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon L");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Sword_L"), TEXT("Part_Sword_L"), &swordLDesc), E_FAIL);

    _swordL = dynamic_pointer_cast<Mon_Dualliste_Sword_L>(_partObjects[TEXT("Part_Sword_L")]);
    CHECK_NULL(_swordL, E_FAIL);

    Mon_Dualliste_Sword_R::SWORD_DESC swordRDesc{};
    swordRDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    swordRDesc.modelType = ModelType::NonAnim;
    swordRDesc.modelTag = L"Model_Dualliste_Sword_R";
    swordRDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon R");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Sword_R"), TEXT("Part_Sword_R"), &swordRDesc), E_FAIL);

    _swordR = dynamic_pointer_cast<Mon_Dualliste_Sword_R>(_partObjects[TEXT("Part_Sword_R")]);
    CHECK_NULL(_swordR, E_FAIL);

    return S_OK;
}

Shared<SL_Dualliste> SL_Dualliste::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SL_Dualliste>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Dualliste");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> SL_Dualliste::Clone(void* arg)
{
    auto clone = make_shared<SL_Dualliste>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void SL_Dualliste::Free()
{
    Character_WorldBase::Free();
}
