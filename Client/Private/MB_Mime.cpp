#include "MB_Mime.h"
#include "Mon_Mime_Body.h"
#include "Mon_Mime_Bagutte.h"
#include "MonsterStatCom.h"
#include "Mon_WeakPoint.h"

IMPLEMENT_REFLECTION(MB_Mime)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MB_Mime";
    info.category = "Monster";
    PROPERTY_FLOAT("Battle Ground Offset Y", _battleGroundOffsetY, -5.f, 5.f, 0.01f);

    return true;
}

MB_Mime::MB_Mime(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_BattleBase(device, context)
{
}

MB_Mime::MB_Mime(const MB_Mime& prototype)
    : Monster_BattleBase(prototype)
{
}

HRESULT MB_Mime::Initialize_Prototype()
{
    return Monster_BattleBase::Initialize_Prototype();
}

HRESULT MB_Mime::Initialize(void* arg)
{
    CHECK_FAILED(Monster_BattleBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    _combatStat->Set_Skill("Mime_AttHamor_3");
    _combatStat->Set_Skill("Mime_AttHand");
    _combatStat->Set_Skill("Mime_Wall");
    _combatStat->Set_Skill("Mime_AttHamor");



    return S_OK;
}

void MB_Mime::Update(float timeDelta)
{
    Monster_BattleBase::Update(timeDelta);

    if (_body)
        _body->Play_Animation(timeDelta);
}

bool MB_Mime::Try_GetDamageFeedbackSocketWorldPosition(Vec3& outWorldPos) const
{
    if (_body == nullptr)
        return false;

    const Matrix* socketMatrix = _body->Get_SocketBoneMatrixPtr("root");
    if (socketMatrix == nullptr)
        return false;

    const Matrix socketWorldMatrix = *socketMatrix * _body->Get_CombinedWorldMatrix();
    outWorldPos = Vec3(socketWorldMatrix._41, socketWorldMatrix._42, socketWorldMatrix._43);
    return true;
}

HRESULT MB_Mime::Ready_PartObjects()
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

Shared<MB_Mime> MB_Mime::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MB_Mime>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : MB_Mime");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MB_Mime::Clone(void* arg)
{
    auto clone = make_shared<MB_Mime>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : MB_Mime");
        return nullptr;
    }

    return clone;
}

void MB_Mime::Free()
{
    Monster_BattleBase::Free();
}
