#include "MW_Dualliste.h"

#include "ClientInstance.h"
#include "Client_Constants.h"
#include "Mon_Dualliste_Body.h"
#include "Mon_Dualliste_Sword_L.h"
#include "Mon_Dualliste_Sword_R.h"
#include "WeaponPrefabLoader.h"

IMPLEMENT_REFLECTION(MW_Dualliste)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_Dualliste";
    info.category = "Monster";

    return true;
}

MW_Dualliste::MW_Dualliste(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_WorldBase(device, context)
{
    _worldMoveSpeedScale = 0.4f;
}

MW_Dualliste::MW_Dualliste(const MW_Dualliste& prototype)
    : Monster_WorldBase(prototype)
{
}

HRESULT MW_Dualliste::Initialize_Prototype()
{
    return Monster_WorldBase::Initialize_Prototype();
}

HRESULT MW_Dualliste::Initialize(void* arg)
{
    CHECK_FAILED(Monster_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void MW_Dualliste::Priority_Update(float timeDelta)
{
    Monster_WorldBase::Priority_Update(timeDelta);
}

void MW_Dualliste::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Monster_WorldBase::Update(timeDelta);
}

HRESULT MW_Dualliste::Render()
{
    return S_OK;
}

HRESULT MW_Dualliste::Render_Shadow()
{
    return S_OK;
}

void MW_Dualliste::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_WorldBase::OnBeginOverlap(self, other);
}

HRESULT MW_Dualliste::Ready_Components()
{
    CHECK_FAILED(Monster_WorldBase::Ready_Components(), E_FAIL);
    return S_OK;
}

HRESULT MW_Dualliste::Ready_PartObjects()
{
    Mon_Dualliste_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Dualliste";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Dualliste_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    Mon_Dualliste_Sword_L::SWORD_DESC swordLDesc{};
    WeaponPrefabLoader::LoadedWeaponPrefab swordLPrefab{};
    if (!WeaponPrefabLoader::Load(Constants::Prefab::Dualliste_Sword_L, swordLPrefab, "Mon_Dualliste_Sword_L"))
        return E_FAIL;

    swordLDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    swordLDesc.modelType = swordLPrefab.modelType;
    swordLDesc.modelTag = swordLPrefab.modelTag;
    swordLDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon L");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Sword_L"), TEXT("Part_Sword_L"), &swordLDesc), E_FAIL);

    _swordL = dynamic_pointer_cast<Mon_Dualliste_Sword_L>(_partObjects[TEXT("Part_Sword_L")]);
    CHECK_NULL(_swordL, E_FAIL);
    _swordL->From_Json(swordLPrefab.rootJson);
    _swordL->Set_SourcePrefabName(Constants::Prefab::Dualliste_Sword_L);

    Mon_Dualliste_Sword_R::SWORD_DESC swordRDesc{};
    WeaponPrefabLoader::LoadedWeaponPrefab swordRPrefab{};
    if (!WeaponPrefabLoader::Load(Constants::Prefab::Dualliste_Sword_R, swordRPrefab, "Mon_Dualliste_Sword_R"))
        return E_FAIL;

    swordRDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    swordRDesc.modelType = swordRPrefab.modelType;
    swordRDesc.modelTag = swordRPrefab.modelTag;
    swordRDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon R");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Sword_R"), TEXT("Part_Sword_R"), &swordRDesc), E_FAIL);

    _swordR = dynamic_pointer_cast<Mon_Dualliste_Sword_R>(_partObjects[TEXT("Part_Sword_R")]);
    CHECK_NULL(_swordR, E_FAIL);
    _swordR->From_Json(swordRPrefab.rootJson);
    _swordR->Set_SourcePrefabName(Constants::Prefab::Dualliste_Sword_R);

    return S_OK;
}

Shared<MW_Dualliste> MW_Dualliste::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MW_Dualliste>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Dualliste");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MW_Dualliste::Clone(void* arg)
{
    auto clone = make_shared<MW_Dualliste>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MW_Dualliste::Free()
{
    Monster_WorldBase::Free();
}
