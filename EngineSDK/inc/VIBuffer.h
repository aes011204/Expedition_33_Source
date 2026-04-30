#pragma once

#include "Component.h"

/* VIBuffer = Vertex + Index + Buffer */

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer abstract : public Component
{
protected:
	VIBuffer(ComPtr<Device> device, ComPtr<Context> context);
	VIBuffer(const VIBuffer& Prototype);
	virtual ~VIBuffer() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* pArg);
	virtual HRESULT Bind_Resources();
	virtual HRESULT Render();

protected:
	ID3D11Buffer*				_vb = { nullptr };
	ID3D11Buffer*				_ib = { nullptr };

protected:
	uint32						_numVertexBuffers = {};
	uint32						_vertexStride = {};
	uint32						_numVertices = {};

	uint32						_indexStride = {};
	uint32						_numIndices = {};
	D3D11_PRIMITIVE_TOPOLOGY	_primitiveType = {};


public:
	virtual Shared<Component> Clone(void* pArg) = 0;
	virtual void Free() override;
};

NS_END