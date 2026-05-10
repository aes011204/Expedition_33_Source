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
class CombatStatCom;
class PlayerStateMachineCom;

class Player final : public Character
{
    GENERATED_GAMEOBJECT(Player, ObjectType::Player)

public:
	Player(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Player(const Player& prototype);
	virtual ~Player() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;

	void Priority_Update(float timeDelta) override;
	void Update(float timeDelta) override;
	void Late_Update(float timeDelta) override;
	HRESULT Render() override;

protected:
	HRESULT Ready_Components();
	HRESULT Ready_PartObjects();

    HRESULT Ready_Collider();

private:
    PlayerState                     _currentState;

    Shared<MovementCom>             _movementCom;
    Shared<InputCom>                _inputCom;
    Shared<PlayerControllerCom>     _playerControllerCom;
    Shared<PlayerStateMachineCom>   _PlayerStateMachineCom;
    Shared<ColliderCom>             _colliderCom;
    Shared<CombatStatCom>           _combatStatCom;

public:
	static Shared<Player> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END

