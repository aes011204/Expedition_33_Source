#pragma once

#include "Client_Defines.h"
#include "Character.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
class MovementCom;
class ColliderCom;
NS_END

NS_BEGIN(Client)

class InputCom;
class PlayerControllerCom;

class Player final : public Character
{
    GENERATED_GAMEOBJECT(Player, ObjectType::Player)

public:
	Player(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Player(const Player& prototype);
	virtual ~Player() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* arg) override;
    virtual void    BeginPlay() override;

	virtual void Priority_Update(float timeDelta) override;
	virtual void Update(float timeDelta) override;
	virtual void Late_Update(float timeDelta) override;
	virtual HRESULT Render() override;	

protected:
	HRESULT Ready_Components();
	HRESULT Ready_PartObjects();

    HRESULT Ready_Collider();

private:
    Shared<MovementCom>         _movementCom;
    Shared<InputCom>            _inputCom;
    Shared<PlayerControllerCom> _playerControllerCom;
    Shared<ColliderCom>         _colliderCom;

public:
	static Shared<Player> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END

