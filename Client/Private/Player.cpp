#include "Player.h"

#include "Weapon.h"
#include "Body_Player.h"
#include "GameInstance.h"

Player::Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: ContainerObject { device, context }
{
}

Player::Player(const Player& prototype)
	: ContainerObject{ prototype }
{
}

HRESULT Player::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Player::Initialize(void* pArg)
{	
	ContainerObject::CONTAINEROBJECT_DESC		Desc{};
	Desc.speedPerSec = 10.f;
	Desc.degreePerSec = 180.f;

	if (FAILED(__super::Initialize(&Desc)))
		return E_FAIL;
	
	if (FAILED(Ready_Components()))
		return E_FAIL;

	if (FAILED(Ready_PartObjects()))
		return E_FAIL;

	return S_OK;
}

void Player::Priority_Update(float fTimeDelta)
{
	__super::Priority_Update(fTimeDelta);
}

void Player::Update(float fTimeDelta)
{

	if (GAME->KeyPress(KEY_TYPE::DOWN))
	{
		_transformCom->Go_Backward(fTimeDelta);
	}

	if (GAME->KeyPress(KEY_TYPE::LEFT))
	{
		_transformCom->Turn(Vec3::Up, fTimeDelta * -1.f);
	}

	if (GAME->KeyPress(KEY_TYPE::RIGHT))
	{

		_transformCom->Turn(Vec3::Up, fTimeDelta);
	}

	if (GAME->KeyPress(KEY_TYPE::UP))
	{
		_transformCom->Go_Straight(fTimeDelta);

		if(_states & PLAYERSTATE::IDLE)
			_states ^= PLAYERSTATE::IDLE;

		_states |= PLAYERSTATE::WALK;
	}
	else
	{
		if (_states & PLAYERSTATE::WALK)
			_states ^= PLAYERSTATE::WALK;

		_states |= PLAYERSTATE::IDLE;
	}


	__super::Update(fTimeDelta);
}

void Player::Late_Update(float fTimeDelta)
{
	__super::Late_Update(fTimeDelta);
}

HRESULT Player::Render()
{


	return S_OK;
}

HRESULT Player::Ready_Components()
{



	return S_OK;
}

HRESULT Player::Ready_PartObjects()
{
	Body_Player::BODY_PLAYER_DESC		BodyDesc{};
	BodyDesc.pParentState = &_states;
	BodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();

	if (FAILED(__super::Add_PartObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Body_Player"),
		TEXT("Part_Body"), &BodyDesc)))
		return E_FAIL;

	Body_Player* pBody = dynamic_cast<Body_Player*>(_partObjects[TEXT("Part_Body")].get());
	if (nullptr == pBody)
		return E_FAIL;

	Weapon::WEAPON_DESC				WeaponDesc{};
	WeaponDesc.pParentState = &_states;
	WeaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
	WeaponDesc.pSocketMatrix = pBody->Get_SocketBoneMatrixPtr("SWORD");

	if (FAILED(__super::Add_PartObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Weapon"),
		TEXT("Part_Weapon"), &WeaponDesc)))
		return E_FAIL;

	return S_OK;
}


Shared<Player> Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto pInstance = make_shared<Player>(device, context);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Player");
		return nullptr;
	}
	return pInstance;
}


Shared<GameObject> Player::Clone(void* pArg)
{
	auto pInstance = make_shared<Player>(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Player");
		return nullptr;
	}
	return pInstance;
}

void Player::Free()
{
	__super::Free();

}



