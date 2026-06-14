#include "MW_Mime.h"
#include "ClientInstance.h"
#include "Mon_Mime_Body.h"
#include "Mon_Mime_Bagutte.h"
#include "Mon_WeakPoint.h"

IMPLEMENT_REFLECTION(MW_Mime)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_Mime";
    info.category = "Monster";

    return true;
}

MW_Mime::MW_Mime(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_WorldBase(device, context)
{
}

MW_Mime::MW_Mime(const MW_Mime& prototype)
    : Monster_WorldBase(prototype)
{
}

HRESULT MW_Mime::Initialize_Prototype()
{
    return Monster_WorldBase::Initialize_Prototype();
}

HRESULT MW_Mime::Initialize(void* arg)
{
    CHECK_FAILED(Monster_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void MW_Mime::Priority_Update(float timeDelta)
{
    Monster_WorldBase::Priority_Update(timeDelta);
}

void MW_Mime::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Monster_WorldBase::Update(timeDelta);
}

HRESULT MW_Mime::Render()
{
    return S_OK;
}

HRESULT MW_Mime::Render_Shadow()
{
    return S_OK;
}

void MW_Mime::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_WorldBase::OnBeginOverlap(self, other);
}

HRESULT MW_Mime::Ready_Components()
{
    CHECK_FAILED(Monster_WorldBase::Ready_Components(), E_FAIL);
    return S_OK;
}

HRESULT MW_Mime::Ready_PartObjects()
{
    Mon_Mime_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Mime";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Mime_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Mime_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    Mon_Mime_Bagutte::WEAPON_DESC bagutteDesc{};
    bagutteDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bagutteDesc.modelType = ModelType::NonAnim;
    bagutteDesc.modelTag = L"Model_Mime_Bagutte";
    bagutteDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("headSocket");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Mime_Bagutte"), TEXT("Part_Bagutte"), &bagutteDesc), E_FAIL);

    _bagutte = dynamic_pointer_cast<Mon_Mime_Bagutte>(_partObjects[TEXT("Part_Bagutte")]);
    CHECK_NULL(_bagutte, E_FAIL);


    return S_OK;
}

Shared<MW_Mime> MW_Mime::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MW_Mime>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Mime");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MW_Mime::Clone(void* arg)
{
    auto clone = make_shared<MW_Mime>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MW_Mime::Free()
{
    Monster_WorldBase::Free();
}
