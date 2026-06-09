#include "MW_Portier.h"
#include "ClientInstance.h"
#include "Mon_Portier_Body.h"
#include "Mon_WeakPoint.h"

IMPLEMENT_REFLECTION(MW_Portier)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_Portier";
    info.category = "Monster";

    return true;
}

MW_Portier::MW_Portier(const ComPtr<Device>& device, const ComPtr<Context>& context):Monster_WorldBase(device,context)
{
    _worldMoveSpeedScale = 0.1f;
}

MW_Portier::MW_Portier(const MW_Portier& prototype) : Monster_WorldBase(prototype)
{
}

HRESULT MW_Portier::Initialize_Prototype()
{
    return Monster_WorldBase::Initialize_Prototype();
}

HRESULT MW_Portier::Initialize(void* arg)
{
    CHECK_FAILED(Monster_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void MW_Portier::Priority_Update(float timeDelta)
{
    Monster_WorldBase::Priority_Update(timeDelta);
}

void MW_Portier::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Monster_WorldBase::Update(timeDelta);
}

HRESULT MW_Portier::Render()
{
    return S_OK;
}

HRESULT MW_Portier::Render_Shadow()
{
    return S_OK;
}

void MW_Portier::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_WorldBase::OnBeginOverlap(self, other);
}

HRESULT MW_Portier::Ready_Components()
{
   Monster_WorldBase::Ready_Components();
    return S_OK;
}

HRESULT MW_Portier::Ready_PartObjects()
{
    Mon_Portier_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Portier";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Portier_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Portier_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    Mon_WeakPoint::WEAK_DESC weakDesc{};
    weakDesc.parentMatrix = &_body->Get_CombinedWorldMatrix();
    weakDesc.allowMissingModel = true;
    weakDesc.socketName = "Status";
    weakDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Status");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_MonsterWeak"), TEXT("Part_Weak"), &weakDesc), E_FAIL);

    _weak = dynamic_pointer_cast<Mon_WeakPoint>(_partObjects[TEXT("Part_Weak")]);
    CHECK_NULL(_weak, E_FAIL);

    return S_OK;
}

Shared<MW_Portier> MW_Portier::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MW_Portier>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Portier");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MW_Portier::Clone(void* arg)
{
    auto clone = make_shared<MW_Portier>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MW_Portier::Free()
{
    Monster_WorldBase::Free();
}
