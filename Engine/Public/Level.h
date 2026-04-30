#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL Level abstract : public Base
{
public:
	Level(ComPtr<Device> device, ComPtr<Context> context);
	virtual ~Level() = default;

public:
	virtual HRESULT Initialize();
	virtual void Update(float fTimeDelta);
	virtual HRESULT Render();

protected:
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };
public:
	virtual void Free() override;
};

NS_END



