#include "MB_Lancelier.h"

#include "Mon_Lancelier_Body.h"
#include "Mon_Lancelier_Weapon.h"

IMPLEMENT_REFLECTION(MB_Lancelier)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MB_Lancelier";
    info.category = "Monster";

    return true;
}

MB_Lancelier::MB_Lancelier(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_BattleBase(device, context)
{
}

MB_Lancelier::MB_Lancelier(const MB_Lancelier& prototype)
    : Monster_BattleBase(prototype)
{
}

HRESULT MB_Lancelier::Initialize_Prototype()
{
    return Monster_BattleBase::Initialize_Prototype();
}

HRESULT MB_Lancelier::Initialize(void* arg)
{
    CHECK_FAILED(Monster_BattleBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);
    
    _body->Set_Animation(5, 0.3f, true);




    return S_OK;
}

void MB_Lancelier::Update(float timeDelta)
{
    Monster_BattleBase::Update(timeDelta);

    //if (_body && _currentAction != _previousAction)
    //{
    //    switch (_currentAction)
    //    {
    //    case BattleAction::Idle:
    //        _body->Set_Animation(5, 0.3f, true);
    //        break;
    //
    //    case BattleAction::Attack:
    //        _body->Set_Animation(11, 0.3f, false);
    //        break;
    //    }
    //
    //    _previousAction = _currentAction;
    //}

    if (_body)
    {
        _body->Play_Animation(timeDelta);

        // Lancelier 월드 이동은 기존 Monster와 동일하게 body root motion 속도를 사용한다.
        const auto rootVelocity = _body->Get_RootTransformVelocity();
       // _transformCom->Set_SpeedPerSec(rootVelocity.position.z * Constants::Charcter::scaleOffset);
     
        Vec3 pos = _transformCom->Get_Position();
        _transformCom->Set_Position(pos+(rootVelocity.position * 100)); // 현제 fps을 곱해야 하나??

        //LOG_INFO("positionZ :{}", rootVelocity.position.z);
    }
}

//void MB_Lancelier::Decide_Action(Action& curTurnAction, const vector<BattleChar>& heroes)
//{
//    Monster_BattleBase::Decide_Action(curTurnAction, heroes);
//    Set_CurrentAction(BattleAction::Attack); // Lancelier 전용 공격 애니메이션 상태로 전환한다.
//}

HRESULT MB_Lancelier::Ready_PartObjects()
{
    Mon_Lancelier_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Lancelier";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Lancelier_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Lancelier_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    Mon_Lancelier_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = ModelType::NonAnim;
    weaponDesc.modelTag = L"Model_Lancelier_Weapon";
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_R");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Lancelier_Weapon"), TEXT("Part_Weapon"), &weaponDesc), E_FAIL);

    _weapon = dynamic_pointer_cast<Mon_Lancelier_Weapon>(_partObjects[TEXT("Part_Weapon")]);
    CHECK_NULL(_weapon, E_FAIL);

    return S_OK;
}

Shared<MB_Lancelier> MB_Lancelier::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MB_Lancelier>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : MB_Lancelier");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MB_Lancelier::Clone(void* arg)
{
    auto clone = make_shared<MB_Lancelier>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : MB_Lancelier");
        return nullptr;
    }

    return clone;
}

void MB_Lancelier::Free()
{
    Monster_BattleBase::Free();
}
