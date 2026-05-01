#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Light_Manager final : public Base
{
private:
	Light_Manager();

public:
	virtual ~Light_Manager() = default;

public:
	const LIGHT_DESC* Get_LightDesc(uint32 index);

public:
	HRESULT Add_Light(const LIGHT_DESC& lightDesc);

private:
	list<Shared<class Light>>			_lights;

public:
	static Unique<Light_Manager> Create();
	virtual void Free() override;
};

NS_END
