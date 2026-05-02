#pragma once

#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Cube final : public VIBuffer
{
public:
	VIBuffer_Cube(const ComPtr<Device>& device, const ComPtr<Context>& context);
	VIBuffer_Cube(const VIBuffer_Cube& prototype);
	virtual ~VIBuffer_Cube() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* arg);

public:
	static Shared<VIBuffer_Cube> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<Component> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END
