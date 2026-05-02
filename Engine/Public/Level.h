#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL Level abstract : public Base
{
public:
	Level(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual ~Level() = default;

public:
	virtual HRESULT Initialize();
	virtual void Update(float timeDelta);
	virtual HRESULT Render();

protected:
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };
public:
	virtual void Free() override;
};

NS_END



