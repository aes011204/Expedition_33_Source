#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL Component abstract : public Base
{
public:
	Component(ComPtr<Device> device, ComPtr<Context> context);
	Component(const Component& Prototype);
public:
	virtual ~Component() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);

protected:
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };
public:
	virtual Shared<Component> Clone(void* pArg) = 0;
	virtual void Free() override;
};

NS_END





