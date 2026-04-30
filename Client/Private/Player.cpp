#include "Player.h"

#include "Weapon.h"
#include "Body_Player.h"
#include "GameInstance.h"

Player::Player(ComPtr<Device> device, ComPtr<Context> context)
	: ContainerObject { device, context }
{
}

Player::Player(const Player& Prototype)
	: ContainerObject{ Prototype }
{
}

HRESULT Player::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Player::Initialize(void* pArg)
{	
	ContainerObject::CONTAINEROBJECT_DESC		Desc{};
	Desc.fSpeedPerSec = 10.f;
	Desc.fDegreePerSec = 180.f;

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
		_transformCom->Turn(XMVectorSet(0.f, 1.f, 0.f, 0.f), fTimeDelta * -1.f);
	}

	if (GAME->KeyPress(KEY_TYPE::RIGHT))
	{

		_transformCom->Turn(XMVectorSet(0.f, 1.f, 0.f, 0.f), fTimeDelta);
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
	BodyDesc.pParentMatrix = _transformCom->Get_WorldMatrixPtr();

	if (FAILED(__super::Add_PartObject(ETOI(LevelType::GamePlay), TEXT("Prototype_GameObject_Body_Player"),
		TEXT("Part_Body"), &BodyDesc)))
		return E_FAIL;

	Body_Player* pBody = dynamic_cast<Body_Player*>(_partObjects[TEXT("Part_Body")].get());
	if (nullptr == pBody)
		return E_FAIL;

	Weapon::WEAPON_DESC				WeaponDesc{};
	WeaponDesc.pParentState = &_states;
	WeaponDesc.pParentMatrix = _transformCom->Get_WorldMatrixPtr();
	WeaponDesc.pSocketMatrix = pBody->Get_SocketBoneMatrixPtr("SWORD");

	if (FAILED(__super::Add_PartObject(ETOI(LevelType::GamePlay), TEXT("Prototype_GameObject_Weapon"),
		TEXT("Part_Weapon"), &WeaponDesc)))
		return E_FAIL;

	return S_OK;
}


Shared<Player> Player::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	Shared<Player> pInstance = MakeSharedFromRaw(new Player(device, context));

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Player");
		Safe_Release(pInstance);
	}
	return pInstance;
}


Shared<GameObject> Player::Clone(void* pArg)
{
	Shared<Player> pInstance = MakeSharedFromRaw(new Player(*this));

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Player");
		Safe_Release(pInstance);
	}
	return pInstance;
}

void Player::Free()
{
	__super::Free();

}



