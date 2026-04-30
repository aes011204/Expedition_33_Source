#include "VIBuffer_Rect.h"

VIBuffer_Rect::VIBuffer_Rect(ComPtr<Device> device, ComPtr<Context> context)
	: VIBuffer { device, context }
{
}

VIBuffer_Rect::VIBuffer_Rect(const VIBuffer_Rect& Prototype)
	: VIBuffer{ Prototype }
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

	VTXTEX* pVertices = new VTXTEX[_numVertices];
	pVertices[0].vPosition = XMFLOAT3(-0.5f, 0.5f, 0.f);
	pVertices[0].vTexcoord = XMFLOAT2(0.f, 0.f);
	pVertices[1].vPosition = XMFLOAT3(0.5f, 0.5f, 0.f);
	pVertices[1].vTexcoord = XMFLOAT2(1.f, 0.f);
	pVertices[2].vPosition = XMFLOAT3(0.5f, -0.5f, 0.f);
	pVertices[2].vTexcoord = XMFLOAT2(1.f, 1.f);
	pVertices[3].vPosition = XMFLOAT3(-0.5f,-0.5f, 0.f);
	pVertices[3].vTexcoord = XMFLOAT2(0.f, 1.f);

	D3D11_SUBRESOURCE_DATA VertexInitialData{};
	VertexInitialData.pSysMem = pVertices;

	if (FAILED(_device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, &_vb)))
		return E_FAIL;

	D3D11_BUFFER_DESC IndexBufferDesc{};
	IndexBufferDesc.ByteWidth = _indexStride * _numIndices;
	IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IndexBufferDesc.StructureByteStride = _indexStride;

	uint16* pIndices = new uint16[_numIndices];
	pIndices[0] = 0;
	pIndices[1] = 1;
	pIndices[2] = 2;
	pIndices[3] = 0;
	pIndices[4] = 2;
	pIndices[5] = 3;

	D3D11_SUBRESOURCE_DATA IndexInitialData{};
	IndexInitialData.pSysMem = pIndices;

	if (FAILED(_device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, &_ib)))
		return E_FAIL;

	Safe_Delete_Array(pVertices);
	Safe_Delete_Array(pIndices);

	return S_OK;
}

HRESULT VIBuffer_Rect::Initialize(void* pArg)
{
	return S_OK;
}

Shared<VIBuffer_Rect> VIBuffer_Rect::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	Shared<VIBuffer_Rect> pInstance = MakeSharedFromRaw(new VIBuffer_Rect(device, context));

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : VIBuffer_Rect");
		Safe_Release(pInstance);
	}
	return pInstance;
}

Shared<Component> VIBuffer_Rect::Clone(void* pArg)
{
	Shared<VIBuffer_Rect> pInstance = MakeSharedFromRaw(new VIBuffer_Rect(*this));

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : VIBuffer_Rect");
		Safe_Release(pInstance);
	}
	return pInstance;
}

void VIBuffer_Rect::Free()
{
	__super::Free();
}
