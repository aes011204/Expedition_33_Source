#pragma once

#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Cube final : public VIBuffer
{
protected:
	VIBuffer_Cube(ComPtr<Device> device, ComPtr<Context> context);
	VIBuffer_Cube(const VIBuffer_Cube& Prototype);
	virtual ~VIBuffer_Cube() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);

public:
	static Shared<VIBuffer_Cube> Create(ComPtr<Device> device, ComPtr<Context> context);
	virtual Shared<Component> Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END