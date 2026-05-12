#include "Monster.h"
#include "GameInstance.h"
#include "AIController.h"
#include "Battle_Enum.h"
#include "ModelCom.h"
#include "Material_Types.h"
#include "ShaderCom.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "ClientInstance.h"
#include "BlackBoard.h"
#include "Mon_Lancleier_Body.h"
#include "Mon_Lancleier_Weapon.h"

IMPLEMENT_REFLECTION(Monster)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Monster";
    info.category = "Character";

    return true;
}

Monster::Monster(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Character { device, context }
{
}

Monster::Monster(const Monster& prototype)
	: Character { prototype }

{
}

HRESULT Monster::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Monster::Initialize(void* arg)
{
	if (FAILED(__super::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
		return E_FAIL;

	//_transformCom->Set_Position(Vec3(
	//	GAME->Random(0.f, 10.f),
	//	2.f,
	//	GAME->Random(0.f, 10.f)
	//));


    //_currentAction = MonsterAction::Idle;


    _transformCom->Set_Position(Vec3(
        10.f,0.f,10.f
    ));

    //_model->Set_Animation(5, true);

	return S_OK;
}

void Monster::Priority_Update(float timeDelta)
{

}

void Monster::Update(float timeDelta)
{
    if (_aiCtrl)
    {
        Shared<BlackBoard> bb = _aiCtrl->Get_BlackBoard();
        Shared<GameObject> player = CLIENT->Find_Player(ETOI(LevelType::Static));

        if (bb && player)
        {
            const Vec3 playerPos = player->Get_Transform()->Get_Position();
            bb->Set_Value("playerPos", playerPos);
        }

        _aiCtrl->Update(timeDelta);
    }

    if (_aiCtrl && _aiCtrl->Get_IsMoving() == true)
    {
        Set_CurrentAction(MonsterAction::Move);
    }

    if (_body && _currentAction != _previousAction)
    {
        switch (_currentAction)
        {
        case MonsterAction::Idle:
            
            _body->Set_Animation( 5,0.3f, true);
            break;

        case MonsterAction::Move:
            _body->Set_Animation( 11, 0.3f, true);
            break;

        case MonsterAction::Attack:
            _body->Set_Animation( 11, 0.3f, false);
            break;
        }

        _previousAction = _currentAction;
    }

    if (_body)
    {
        _body->Play_Animation(timeDelta);

        auto str = _body->Get_RootTransformVelocity();
        _transformCom->Set_SpeedPerSec((str.position.z * Constants::Charcter::scaleOffset));
    }

   // Matrix ax;
   // Vector3 vec3;
   // Quat a;
   // Vec3 vevec;
   // ax.Decompose(vec3, a, vevec);
    __super::Update(timeDelta);
}


void Monster::Late_Update(float fTimeDelta)
{
    if (_collider)
    {
        _collider->Update_Collider(_transformCom->Get_WorldMatrix());
        GAME->Add_Collider(_collider);
    }
    __super::Late_Update(fTimeDelta);

}

//Action Monster::Decide_BattleAction()
//{
//    Action action{};
//    action._curChar = static_pointer_cast<Character>(shared_from_this());
//    action.type = EBattleContext::BaseAttack;
//    action.target = 0;
//    return action;
//}

HRESULT Monster::Render()
{
    return S_OK;
}

HRESULT Monster::Render_Shadow()
{
    return S_OK;
}

HRESULT Monster::Ready_Components()
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Com_AIController"), _aiCtrl)))
        return E_FAIL;

    _aiCtrl->Set_Owner(static_pointer_cast<Monster>(shared_from_this()));

    CHECK_FAILED(Ready_Collider(), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}
HRESULT Monster::Ready_PartObjects()
{
    Mon_Lancleier_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Lancleier";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Lancleier_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Lancleier_Body>(_partObjects[TEXT("Part_Body")]);
    if (nullptr == _body)
        return E_FAIL;

    Mon_Lancleier_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = ModelType::NonAnim;
    weaponDesc.modelTag = L"Model_Lancleier_Weapon";
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_R");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Lancleier_Weapon"), TEXT("Part_Weapon"), &weaponDesc), E_FAIL);

    _weapon = dynamic_pointer_cast<Mon_Lancleier_Weapon>(_partObjects[TEXT("Part_Weapon")]);
    if (nullptr == _weapon)
        return E_FAIL;

    return S_OK;
}


HRESULT Monster::Ready_Collider()
{
    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.5f, 0.f);
    obbDesc.extents = Vec3(0.5f, 0.6f, 0.5f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Com_Collider_OBB", _collider, &obbDesc), E_FAIL);
    _collider->Set_CollisionPreset(Collision_Preset::Monster_Body);
    _collider->Set_IsActive(true);

    return S_OK;
}

Shared<Monster> Monster::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Monster>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Monster");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Monster::Clone(void* arg)
{
    auto clone = make_shared<Monster>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
       // MSG_BOX("Failed to Cloned : Monster");
        return nullptr;
    }

    return clone;
}

void Monster::Free()
{
	__super::Free();

}


