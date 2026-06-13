#include "MW_Bishop.h"
#include "ClientInstance.h"
#include "Mon_Bishop_Body.h"
#include "Mon_Lancelier_Weapon.h"
#include "MonsterStatCom.h"
#include "Mon_WeakPoint.h"

IMPLEMENT_REFLECTION(MW_Bishop)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_Bishop";
    info.category = "Monster";

    return true;
}


MW_Bishop::MW_Bishop(const ComPtr<Device>& device, const ComPtr<Context>& context) :Monster_WorldBase(device, context)
{
    _worldMoveSpeedScale = 0.4f;
}

MW_Bishop::MW_Bishop(const MW_Bishop& prototype) : Monster_WorldBase(prototype)
{
}

HRESULT MW_Bishop::Initialize_Prototype()
{
    return Monster_WorldBase::Initialize_Prototype();
}

HRESULT MW_Bishop::Initialize(void* arg)
{
    CHECK_FAILED(Monster_WorldBase::Initialize(arg), E_FAIL);

    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void MW_Bishop::Priority_Update(float timeDelta)
{
    Monster_WorldBase::Priority_Update(timeDelta);
}

void MW_Bishop::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Monster_WorldBase::Update(timeDelta);
}

HRESULT MW_Bishop::Render()
{
    return S_OK;
}

HRESULT MW_Bishop::Render_Shadow()
{
    return S_OK;
}

void MW_Bishop::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_WorldBase::OnBeginOverlap(self, other);
}

HRESULT MW_Bishop::Ready_Components()
{
    Monster_WorldBase::Ready_Components();
    return S_OK;
}

HRESULT MW_Bishop::Ready_PartObjects()
{
    Mon_Bishop_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Bishop";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Bishop_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Bishop_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    Mon_WeakPoint::WEAK_DESC weakDesc{};
    weakDesc.parentMatrix = &_body->Get_CombinedWorldMatrix();
    weakDesc.allowMissingModel = true;
    weakDesc.socketName = "Aim_Target";
    weakDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Aim_Target");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_MonsterWeak"), TEXT("Part_Weak"), &weakDesc), E_FAIL);

    _weak = dynamic_pointer_cast<Mon_WeakPoint>(_partObjects[TEXT("Part_Weak")]);
    CHECK_NULL(_weak, E_FAIL);

    return S_OK;
}

Shared<MW_Bishop> MW_Bishop::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MW_Bishop>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Bishop");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MW_Bishop::Clone(void* arg)
{
    auto clone = make_shared<MW_Bishop>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MW_Bishop::Free()
{
    Monster_WorldBase::Free();
}
