#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL Component abstract : public Base
{
public:
	Component(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Component(const Component& prototype);
public:
	virtual ~Component() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* arg);

protected:
	ComPtr<Device>  _device = { nullptr };
	ComPtr<Context> _context = { nullptr };
public:
	virtual Shared<Component> Clone(void* arg) = 0;
	virtual void Free() override;
};

NS_END





