#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Layer final : public Base
{
public:
	Layer();
	virtual ~Layer() { Free(); }

public:
	HRESULT Add_GameObject(Shared<class GameObject> pGameObject);
	void Priority_Update(float fTimeDelta);
	void Update(float fTimeDelta);
	void Late_Update(float fTimeDelta);

private:
	list<Shared<class GameObject>> _gameObjects;

public:
	static Shared<Layer> Create();
	virtual void Free() override;
};

NS_END

