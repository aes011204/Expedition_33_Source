#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Light final : public Base
{
public:
	Light();
	virtual ~Light() { Free(); }

public:
	const LIGHT_DESC* Get_LightDesc() const {
		return &_desc;
	}

public:
	HRESULT Initialize(const LIGHT_DESC& LightDesc);

private:
	LIGHT_DESC					_desc{};

public:
	static Shared<Light> Create(const LIGHT_DESC& LightDesc);
	virtual void Free() override;
};

NS_END
