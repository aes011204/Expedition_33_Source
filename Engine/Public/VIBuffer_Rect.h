#pragma once

#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Rect final : public VIBuffer
{
protected:
	VIBuffer_Rect(ComPtr<Device> device, ComPtr<Context> context);
	VIBuffer_Rect(const VIBuffer_Rect& Prototype);
	virtual ~VIBuffer_Rect() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);

public:
	static Shared<VIBuffer_Rect> Create(ComPtr<Device> device, ComPtr<Context> context);
	virtual Shared<Component> Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END