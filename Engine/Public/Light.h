#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Light final : public Base
{
public:
	Light();
	virtual ~Light() = default;

public:
	const LIGHT_DESC* Get_LightDesc() const {
		return &_desc;
	}

public:
	HRESULT Initialize(const LIGHT_DESC& lightDesc);

private:
	LIGHT_DESC					_desc{};

public:
	static Shared<Light> Create(const LIGHT_DESC& lightDesc);
	virtual void Free() override;
};

NS_END
