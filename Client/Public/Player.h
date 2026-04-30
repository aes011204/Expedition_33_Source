#pragma once

#include "Client_Defines.h"
#include "ContainerObject.h"

NS_BEGIN(Engine)
class Shader;
class Model;
NS_END

NS_BEGIN(Client)

class Player final : public ContainerObject
{
public:
	enum PLAYERSTATE { 
		IDLE	= 0x00000001, 
		WALK	= 0x00000002, 
		ATTACK	= 0x00000004, 		
	};
private:
	Player(ComPtr<Device> device, ComPtr<Context> context);
	Player(const Player& Prototype);
	virtual ~Player() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* pArg) override;
	virtual void Priority_Update(float fTimeDelta) override;
	virtual void Update(float fTimeDelta) override;
	virtual void Late_Update(float fTimeDelta) override;
	virtual HRESULT Render() override;	

protected:
	HRESULT Ready_Components();
	HRESULT Ready_PartObjects();

private:
	uint32			_states = {};

public:
	static Shared<Player> Create(ComPtr<Device> device, ComPtr<Context> context);
	virtual Shared<GameObject> Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END

