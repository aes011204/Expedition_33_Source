#include "MB_Portier.h"
#include "Mon_Portier_Body.h"
#include "MonsterStatCom.h"
#include "Mon_WeakPoint.h"

IMPLEMENT_REFLECTION(MB_Portier)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MB_Portier";
    info.category = "Monster";
    PROPERTY_FLOAT("Battle Ground Offset Y", _battleGroundOffsetY, -5.f, 5.f, 0.01f);
    PROPERTY_FLOAT("Battle RunLoop Move Speed", _battleRunLoopMoveSpeed, 0.f, 30.f, 0.1f);

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

        //for (int i = 0; i < _body->Get_Model()->Get_NumMaterials(); i++)
        //{
        //    auto cb = _body->Get_Model()->Get_Material(i)->Get_CB();
        //    cb.emissiveIntensity = 0;
        //    _body->Get_Model()->Get_Material(i)->Set_CB(cb);
        //}
}

bool MB_Portier::Try_GetDamageFeedbackSocketWorldPosition(Vec3& outWorldPos) const
{
    if (_body == nullptr)
        return false;

    const Matrix* socketMatrix = _body->Get_SocketBoneMatrixPtr("Status");
    if (socketMatrix == nullptr)
        return false;

    const Matrix socketWorldMatrix = *socketMatrix * _body->Get_CombinedWorldMatrix();
    outWorldPos = Vec3(socketWorldMatrix._41, socketWorldMatrix._42, socketWorldMatrix._43);
    return true;
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

    Mon_WeakPoint::WEAK_DESC weakDesc{};
    weakDesc.parentMatrix = &_body->Get_CombinedWorldMatrix();
    weakDesc.allowMissingModel = true;
    weakDesc.socketName = "Status";
    weakDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Status");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_MonsterWeak"), TEXT("Part_Weak"), &weakDesc), E_FAIL); // 약점 파츠는 모델 없이 neck_02 소켓 기준으로 붙인다.

    _weak = dynamic_pointer_cast<Mon_WeakPoint>(_partObjects[TEXT("Part_Weak")]);
    CHECK_NULL(_weak, E_FAIL);


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
