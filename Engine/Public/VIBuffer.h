#pragma once

#include "Component.h"

/* VIBuffer = Vertex + Index + Buffer */

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer abstract : public Component
{
public:
	VIBuffer(const ComPtr<Device>& device, const ComPtr<Context>& context);
	VIBuffer(const VIBuffer& prototype);
	virtual ~VIBuffer() = default;

public:
	virtual HRESULT Initialize_Prototype();
	virtual HRESULT Initialize(void* arg);
	virtual HRESULT Bind_Resources();
	virtual HRESULT Render();

	uint32 Get_NumIndices() const { return _numIndices; }
    // Profile View의 리소스 요약에서 vertex/index buffer의 대략적인 GPU byte를 추정할 때 사용한다.
    size_t Get_DebugEstimatedBufferBytes() const;

protected:
	ComPtr<ID3D11Buffer>		_vb = nullptr;
	ComPtr<ID3D11Buffer>		_ib = nullptr;

protected:
	uint32						_numVertexBuffers = {};
	uint32						_vertexStride = {};
	uint32						_numVertices = {};

	uint32						_indexStride = {};
	uint32						_numIndices = {};
	D3D11_PRIMITIVE_TOPOLOGY	_primitiveType = {};


public:
	virtual Shared<Component> Clone(void* arg) = 0;
	virtual void Free() override;
};

NS_END
