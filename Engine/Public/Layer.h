#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Layer final : public Base
{
public:
	Layer();
	virtual ~Layer() { Free(); }

public:
	HRESULT Add_GameObject(Shared<class GameObject> gameObject);
	void Priority_Update(float timeDelta);
	void Update(float timeDelta);
	void Late_Update(float timeDelta);

private:
	list<Shared<class GameObject>> _gameObjects;

public:
	static Shared<Layer> Create();
	virtual void Free() override;
};

NS_END

