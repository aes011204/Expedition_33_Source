#include "Player.h"
#include "Weapon.h"
#include "Body_Player.h"
#include "GameInstance.h"
#include "InputCom.h"
#include "MovementCom.h"
#include "PlayerControllerCom.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"

IMPLEMENT_REFLECTION(Player)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Player";
    info.category = "Character";

    PROPERTY_ENUM("PlayerState",_testPlayerState, PlayerState);

    return true;
}

Player::Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Character { device, context }
{
}

Player::Player(const Player& prototype)
	: Character{ prototype }
{
}

HRESULT Player::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Player::Initialize(void* arg)
{	
	Character::ContainerObjectDesc desc{};
	desc.speedPerSec = 10.f;
	desc.degreePerSec = 180.f;

	if (FAILED(__super::Initialize(&desc)))
		return E_FAIL;
	
	if (FAILED(Ready_Components()))
		return E_FAIL;

	if (FAILED(Ready_PartObjects()))
		return E_FAIL;

	return S_OK;
}

void Player::BeginPlay()
{
    Character::BeginPlay();

}

void Player::Priority_Update(float timeDelta)
{
	__super::Priority_Update(timeDelta);
}

void Player::Update(float timeDelta)
{
	__super::Update(timeDelta);

    _playerControllerCom->Update_Controller(timeDelta);
}

void Player::Late_Update(float timeDelta)
{
	__super::Late_Update(timeDelta);

    if (_collider)
    {
        _collider->Update_Collider(_transformCom->Get_WorldMatrix());
        GAME->Add_Collider(_collider);
    }
}

HRESULT Player::Render()
{

	return S_OK;
}

HRESULT Player::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Input"), _inputCom), E_FAIL);

    MovementCom::MoveDesc moveDesc{};
    moveDesc.walkSpeed = 4.f;
    moveDesc.runSpeed = 7.f;
    moveDesc.turnSpeedDegree = 180.f;
    moveDesc.jumpVelocity = 8.f;
    moveDesc.gravity = -20.f;
    moveDesc.groundY = 0.f;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Movement"), _movementCom, &moveDesc), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerController"), _playerControllerCom), E_FAIL);

    CHECK_FAILED(Ready_Collider(), E_FAIL);


    return S_OK;
}

HRESULT Player::Ready_PartObjects()
{
	Body_Player::BODY_PLAYER_DESC		BodyDesc{};
	//BodyDesc.pParentState = &_states;
	BodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();

	if (FAILED(__super::Add_PartObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Body_Player"),
		TEXT("Part_Body"), &BodyDesc)))
		return E_FAIL;

	Body_Player* pBody = dynamic_cast<Body_Player*>(_partObjects[TEXT("Part_Body")].get());
	if (nullptr == pBody)
		return E_FAIL;

	Weapon::WEAPON_DESC				WeaponDesc{};
	//WeaponDesc.pParentState = &_states;
	WeaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
	WeaponDesc.pSocketMatrix = pBody->Get_SocketBoneMatrixPtr("SWORD");

	if (FAILED(__super::Add_PartObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Weapon"),
		TEXT("Part_Weapon"), &WeaponDesc)))
		return E_FAIL;

	return S_OK;
}

HRESULT Player::Ready_Collider()
{
    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.5f, 0.f);
    obbDesc.extents = Vec3(0.5f, 0.6f, 0.5f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Com_Collider_OBB", _collider, &obbDesc), E_FAIL);
    _collider->Set_CollisionPreset(Collision_Preset::Player_Body);
    _collider->Set_IsActive(true);

    return S_OK;
}

Shared<Player> Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<Player>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Player");
		return nullptr;
	}
	return instance;
}


Shared<GameObject> Player::Clone(void* pArg)
{
	auto instance = make_shared<Player>(*this);

	if (FAILED(instance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Player");
		return nullptr;
	}
	return instance;
}

void Player::Free()
{
	__super::Free();

}



