#pragma once

#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Terrain final : public VIBuffer
{
public:
	VIBuffer_Terrain(const ComPtr<Device>& device, const ComPtr<Context>& context);
	VIBuffer_Terrain(const VIBuffer_Terrain& prototype);
	virtual ~VIBuffer_Terrain() = default;

public:
	virtual HRESULT Initialize_Prototype(const wchar_t* heightMapFilePath);
	virtual HRESULT Initialize(void* arg) override;

private:
	uint32 _numVerticesX = {};
	uint32 _numVerticesZ = {};

public:
	static Shared<VIBuffer_Terrain> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const wchar_t* heightMapFilePath);
	virtual Shared<Component> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END
