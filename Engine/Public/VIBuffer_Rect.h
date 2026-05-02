#pragma once

#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Rect final : public VIBuffer
{
public:
	VIBuffer_Rect(const ComPtr<Device>& device, const ComPtr<Context>& context);
	VIBuffer_Rect(const VIBuffer_Rect& prototype);
	virtual ~VIBuffer_Rect() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* arg);

public:
	static Shared<VIBuffer_Rect> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<Component> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END
