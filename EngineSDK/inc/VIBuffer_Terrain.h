#pragma once

#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Terrain final : public VIBuffer
{
protected:
	VIBuffer_Terrain(ComPtr<Device> device, ComPtr<Context> context);
	VIBuffer_Terrain(const VIBuffer_Terrain& Prototype);
	virtual ~VIBuffer_Terrain() = default;

public:
	virtual HRESULT Initialize_Prototype(const wchar_t* pHeightMapFilePath);
	virtual HRESULT Initialize(void* pArg) override;

private:
	uint32 _numVerticesX = {};
	uint32 _numVerticesZ = {};

public:
	static Shared<VIBuffer_Terrain> Create(ComPtr<Device> device, ComPtr<Context> context, const wchar_t* pHeightMapFilePath);
	virtual Shared<Component> Clone(void* pArg) override;
	virtual void Free() override;
};

NS_END
