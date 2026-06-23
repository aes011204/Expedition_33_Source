#include "MW_Simon.h"
#include "ClientInstance.h"
#include "Mon_Simon_Body.h"
#include "Mon_Simon_Hair.h"
#include "Mon_Simon_Weapon.h"
#include "MonsterStatCom.h"
#include "WeaponPrefabLoader.h"

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

bool MW_Simon::Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const
{
    return partTag != TEXT("Part_Weapon");
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

    Mon_Simon_Hair::HEAD_DESC headDesc{};
    headDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    headDesc.modelType = ModelType::Anim;
    headDesc.modelTag = L"Model_Simon_Hair";
    headDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("headSocket");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Simon_Hair"), TEXT("Part_Hair"), &headDesc), E_FAIL);

    _hair = dynamic_pointer_cast<Mon_Simon_Hair>(_partObjects[TEXT("Part_Hair")]);
    CHECK_NULL(_hair, E_FAIL);

    WeaponPrefabLoader::LoadedWeaponPrefab loadedPrefab{};
    if (!WeaponPrefabLoader::Load(Constants::Prefab::Simon_Weapon, loadedPrefab, "Mon_Simon_Weapon"))
        return E_FAIL;

    Mon_Simon_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = loadedPrefab.modelType;
    weaponDesc.modelTag = loadedPrefab.modelTag;
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_R");

    CHECK_NULL(weaponDesc.socketMatrix, E_FAIL);

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Simon_Weapon"), TEXT("Part_Weapon"), &weaponDesc), E_FAIL);

    _weapon = dynamic_pointer_cast<Mon_Simon_Weapon>(_partObjects[TEXT("Part_Weapon")]);
    CHECK_NULL(_weapon, E_FAIL);

    _weapon->From_Json(loadedPrefab.rootJson);
    _weapon->Set_SourcePrefabName(Constants::Prefab::Simon_Weapon);

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
