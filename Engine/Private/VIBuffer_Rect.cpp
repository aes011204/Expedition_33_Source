#include "VIBuffer_Rect.h"

VIBuffer_Rect::VIBuffer_Rect(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: VIBuffer { device, context }
{
}

VIBuffer_Rect::VIBuffer_Rect(const VIBuffer_Rect& prototype)
	: VIBuffer{ prototype }
{
}

HRESULT VIBuffer_Rect::Initialize_Prototype()
{
	_numVertexBuffers = 1;
	_numVertices = 4;
	_vertexStride = sizeof(VTXTEX);
	_numIndices = 6;
	_indexStride = 2;
	_primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	D3D11_BUFFER_DESC VertexBufferDesc{};
	VertexBufferDesc.ByteWidth = _vertexStride * _numVertices;
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VertexBufferDesc.StructureByteStride = _vertexStride;

	VTXTEX* vertices = new VTXTEX[_numVertices];
	vertices[0].position = XMFLOAT3(-0.5f, 0.5f, 0.f);
	vertices[0].texcoord = XMFLOAT2(0.f, 0.f);
	vertices[1].position = XMFLOAT3(0.5f, 0.5f, 0.f);
	vertices[1].texcoord = XMFLOAT2(1.f, 0.f);
	vertices[2].position = XMFLOAT3(0.5f, -0.5f, 0.f);
	vertices[2].texcoord = XMFLOAT2(1.f, 1.f);
	vertices[3].position = XMFLOAT3(-0.5f,-0.5f, 0.f);
	vertices[3].texcoord = XMFLOAT2(0.f, 1.f);

	D3D11_SUBRESOURCE_DATA VertexInitialData{};
	VertexInitialData.pSysMem = vertices;

	if (FAILED(_device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, &_vb)))
		return E_FAIL;

	D3D11_BUFFER_DESC IndexBufferDesc{};
	IndexBufferDesc.ByteWidth = _indexStride * _numIndices;
	IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IndexBufferDesc.StructureByteStride = _indexStride;

	uint16* indices = new uint16[_numIndices];
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 0;
	indices[4] = 2;
	indices[5] = 3;

	D3D11_SUBRESOURCE_DATA IndexInitialData{};
	IndexInitialData.pSysMem = indices;

	if (FAILED(_device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, &_ib)))
		return E_FAIL;

	Safe_Delete_Array(vertices);
	Safe_Delete_Array(indices);

	return S_OK;
}

HRESULT VIBuffer_Rect::Initialize(void* arg)
{
	return S_OK;
}

Shared<VIBuffer_Rect> VIBuffer_Rect::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<VIBuffer_Rect>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : VIBuffer_Rect");
		return nullptr;
	}
	return instance;
}

Shared<Component> VIBuffer_Rect::Clone(void* arg)
{
	auto instance = make_shared<VIBuffer_Rect>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : VIBuffer_Rect");
		return nullptr;
	}
	return instance;
}

void VIBuffer_Rect::Free()
{
	__super::Free();
}
