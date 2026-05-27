#include "MB_Portier.h"
#include "Mon_Portier_Body.h"
#include "MonsterStatCom.h"

IMPLEMENT_REFLECTION(MB_Portier)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MB_Portier";
    info.category = "Monster";

    return true;
}

MB_Portier::MB_Portier(const ComPtr<Device>& device, const ComPtr<Context>& context):Monster_BattleBase(device,context)
{
}

MB_Portier::MB_Portier(const MB_Portier& prototype): Monster_BattleBase(prototype)
{
}

HRESULT MB_Portier::Initialize_Prototype()
{
    return Monster_BattleBase::Initialize_Prototype();
}

HRESULT MB_Portier::Initialize(void* arg)
{
    CHECK_FAILED(Monster_BattleBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

   // _body->Set_Animation("Portier_Idle", 0.3f, true);

    _combatStat->Set_Skill("Portier_AttackAoE");
    _combatStat->Set_Skill("Portier_JumpHit");

    return S_OK;
}

void MB_Portier::Update(float timeDelta)
{
    Monster_BattleBase::Update(timeDelta);
    

        if (_body)
        {
            _body->Play_Animation(timeDelta);
        }
}

HRESULT MB_Portier::Ready_PartObjects()
{
    Mon_Portier_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Portier";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Portier_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Portier_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    return S_OK;
}

Shared<MB_Portier> MB_Portier::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MB_Portier>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : MB_Lancelier");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MB_Portier::Clone(void* arg)
{
    auto clone = make_shared<MB_Portier>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : MB_Lancelier");
        return nullptr;
    }

    return clone;
}

void MB_Portier::Free()
{
    Monster_BattleBase::Free();
}
