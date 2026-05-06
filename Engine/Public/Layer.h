#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class GameObject;

class Layer final : public Base
{
public:
	Layer();
	virtual ~Layer() = default;

public:
	HRESULT Add_GameObject(Shared<GameObject> gameObject);
    bool    Remove_GameObject(const Shared<GameObject>& gameObject);
	void    Priority_Update(float timeDelta);
	void    Update(float timeDelta);
	void    Late_Update(float timeDelta);

public:
    Shared<GameObject> Find_FirstObject();

    const list<Shared<GameObject>>& Get_GameObjects() const { return _gameObjects; }

private:
	list<Shared<GameObject>> _gameObjects;

public:
	static Shared<Layer> Create();
	virtual void Free() override;
};

NS_END

