#include "VIBuffer_Cube.h"

VIBuffer_Cube::VIBuffer_Cube(ComPtr<Device> device, ComPtr<Context> context)
	: VIBuffer { device, context }
{
}

VIBuffer_Cube::VIBuffer_Cube(const VIBuffer_Cube& Prototype)
	: VIBuffer{ Prototype }
{
}

HRESULT VIBuffer_Cube::Initialize_Prototype()
{
	_numVertexBuffers = 1;
	_numVertices = 8;
	_vertexStride = sizeof(VTXCUBE);
	_numIndices = 36;
	_indexStride = 2;
	_primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	D3D11_BUFFER_DESC VertexBufferDesc{};
	VertexBufferDesc.ByteWidth = _vertexStride * _numVertices;
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VertexBufferDesc.StructureByteStride = _vertexStride;

	VTXCUBE* pVertices = new VTXCUBE[_numVertices];
	pVertices[0].vPosition = XMFLOAT3(-0.5f, 0.5f, -0.5f);
	pVertices[0].vTexcoord = pVertices[0].vPosition;
	pVertices[1].vPosition = XMFLOAT3(0.5f, 0.5f, -0.5f);
	pVertices[1].vTexcoord = pVertices[1].vPosition;
	pVertices[2].vPosition = XMFLOAT3(0.5f, -0.5f, -0.5f);
	pVertices[2].vTexcoord = pVertices[2].vPosition;
	pVertices[3].vPosition = XMFLOAT3(-0.5f,-0.5f, -0.5f);
	pVertices[3].vTexcoord = pVertices[3].vPosition;
	pVertices[4].vPosition = XMFLOAT3(-0.5f, 0.5f, 0.5f);
	pVertices[4].vTexcoord = pVertices[4].vPosition;
	pVertices[5].vPosition = XMFLOAT3(0.5f, 0.5f, 0.5f);
	pVertices[5].vTexcoord = pVertices[5].vPosition;
	pVertices[6].vPosition = XMFLOAT3(0.5f, -0.5f, 0.5f);
	pVertices[6].vTexcoord = pVertices[6].vPosition;
	pVertices[7].vPosition = XMFLOAT3(-0.5f, -0.5f, 0.5f);
	pVertices[7].vTexcoord = pVertices[7].vPosition;

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
	pIndices[0] = 1; pIndices[1] = 5; pIndices[2] = 6;
	pIndices[3] = 1; pIndices[4] = 6; pIndices[5] = 2;
	pIndices[6] = 4; pIndices[7] = 0; pIndices[8] = 3;
	pIndices[9] = 4; pIndices[10] = 3; pIndices[11] = 7;
	pIndices[12] = 4; pIndices[13] = 5; pIndices[14] = 1;
	pIndices[15] = 4; pIndices[16] = 1; pIndices[17] = 0;
	pIndices[18] = 3; pIndices[19] = 2; pIndices[20] = 6;
	pIndices[21] = 3; pIndices[22] = 6; pIndices[23] = 7;
	pIndices[24] = 7; pIndices[25] = 6; pIndices[26] = 5;
	pIndices[27] = 7; pIndices[28] = 5; pIndices[29] = 4;
	pIndices[30] = 0; pIndices[31] = 1; pIndices[32] = 2;
	pIndices[33] = 0; pIndices[34] = 2; pIndices[35] = 3;

	D3D11_SUBRESOURCE_DATA IndexInitialData{};
	IndexInitialData.pSysMem = pIndices;

	if (FAILED(_device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, &_ib)))
		return E_FAIL;

	Safe_Delete_Array(pVertices);
	Safe_Delete_Array(pIndices);

	return S_OK;
}

HRESULT VIBuffer_Cube::Initialize(void* pArg)
{
	return S_OK;
}

Shared<VIBuffer_Cube> VIBuffer_Cube::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	auto pInstance = make_shared<VIBuffer_Cube>(device, context);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : VIBuffer_Cube");
		return nullptr;
	}
	return pInstance;
}

Shared<Component> VIBuffer_Cube::Clone(void* pArg)
{
	auto pInstance = make_shared<VIBuffer_Cube>(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : VIBuffer_Cube");
		return nullptr;
	}
	return pInstance;
}

void VIBuffer_Cube::Free()
{
	__super::Free();
}
