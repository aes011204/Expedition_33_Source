#include "MW_Volester.h"
#include "ClientInstance.h"
#include "Mon_Volester_Body.h"

IMPLEMENT_REFLECTION(MW_Volester)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_Volester";
    info.category = "Monster";

    return true;
}


MW_Volester::MW_Volester(const ComPtr<Device>& device, const ComPtr<Context>& context) :Monster_WorldBase(device, context)
{
    _worldMoveSpeedScale = 0.4f;
}

MW_Volester::MW_Volester(const MW_Volester& prototype) : Monster_WorldBase(prototype)
{
}

HRESULT MW_Volester::Initialize_Prototype()
{
    return Monster_WorldBase::Initialize_Prototype();
}

HRESULT MW_Volester::Initialize(void* arg)
{
    CHECK_FAILED(Monster_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void MW_Volester::Priority_Update(float timeDelta)
{
    Monster_WorldBase::Priority_Update(timeDelta);
}

void MW_Volester::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Monster_WorldBase::Update(timeDelta);
}

HRESULT MW_Volester::Render()
{
    return S_OK;
}

HRESULT MW_Volester::Render_Shadow()
{
    return S_OK;
}

void MW_Volester::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_WorldBase::OnBeginOverlap(self, other);
}

HRESULT MW_Volester::Ready_Components()
{
    Monster_WorldBase::Ready_Components();
    return S_OK;
}

HRESULT MW_Volester::Ready_PartObjects()
{
    Mon_Volester_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Volester";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Volester_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Volester_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    return S_OK;
}

Shared<MW_Volester> MW_Volester::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MW_Volester>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Volester");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MW_Volester::Clone(void* arg)
{
    auto clone = make_shared<MW_Volester>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MW_Volester::Free()
{
    Monster_WorldBase::Free();
}
