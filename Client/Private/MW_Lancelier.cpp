#include "MW_Lancelier.h"
#include "ClientInstance.h"
#include "AIController.h"
#include "GameInstance.h"
#include "Mon_Lancelier_Body.h"
#include "Mon_Lancelier_Weapon.h"

IMPLEMENT_REFLECTION(MW_Lancelier)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_Lancelier";
    info.category = "Monster";

    return true;
}

MW_Lancelier::MW_Lancelier(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_WorldBase(device, context)
{
}

MW_Lancelier::MW_Lancelier(const MW_Lancelier& prototype)
    : Monster_WorldBase(prototype)
{
}

HRESULT MW_Lancelier::Initialize_Prototype()
{
    return Monster_WorldBase::Initialize_Prototype();
}

HRESULT MW_Lancelier::Initialize(void* arg)
{
    CHECK_FAILED(Monster_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void MW_Lancelier::Priority_Update(float timeDelta)
{
    Monster_WorldBase::Priority_Update(timeDelta);


}

void MW_Lancelier::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Monster_WorldBase::Update(timeDelta);

    if (_aiCtrl && _aiCtrl->Get_IsMoving())
        Set_CurrentAction(MonsterAction::Move);

    if (_body && _currentAction != _previousAction)
    {
        switch (_currentAction)
        {
        case MonsterAction::Idle:
            _body->Set_Animation("Lancelier_Idle", 0.3f, true);
            break;

        case MonsterAction::Move:
            _body->Set_Animation("Lancelier_Walk", 0.3f, true);
            break;

        case MonsterAction::Attack:
            _body->Set_Animation("Lancelier_Idle",  0.3f, false);
            break;
        }

        _previousAction = _currentAction;
    }

    if (_body)
    {
        _body->Play_Animation(timeDelta);

        // Lancelier 월드 이동은 기존 Monster와 동일하게 body root motion 속도를 사용한다.
        const auto rootVelocity = _body->Get_RootTransformVelocity();
        _transformCom->Set_SpeedPerSec(rootVelocity.position.z *0.4f/*Constants::Charcter::scaleOffset*/);
    }
}

HRESULT MW_Lancelier::Render()
{
    return S_OK;
}

HRESULT MW_Lancelier::Render_Shadow()
{
    return S_OK;
}

void MW_Lancelier::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_WorldBase::OnBeginOverlap(self, other);

}

void MW_Lancelier::Set_BattleMonsterPrefabNames(vector<string>& outPrefabNames) const
{
    outPrefabNames.clear();

    // 일단 렌서러 3마리 고정
    outPrefabNames.push_back(Constants::Prefab::MB_Lancelier);
    outPrefabNames.push_back(Constants::Prefab::MB_Lancelier);
    outPrefabNames.push_back(Constants::Prefab::MB_Lancelier);
}

HRESULT MW_Lancelier::Ready_Components()
{
    CHECK_FAILED(Monster_WorldBase::Ready_Components(), E_FAIL);

    return S_OK;
}

HRESULT MW_Lancelier::Ready_PartObjects()
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

Shared<MW_Lancelier> MW_Lancelier::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MW_Lancelier>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Lancelier");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MW_Lancelier::Clone(void* arg)
{
    auto clone = make_shared<MW_Lancelier>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MW_Lancelier::Free()
{
    Monster_WorldBase::Free();
}
