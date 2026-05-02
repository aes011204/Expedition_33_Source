#include "VIBuffer_Cube.h"

VIBuffer_Cube::VIBuffer_Cube(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: VIBuffer { device, context }
{
}

VIBuffer_Cube::VIBuffer_Cube(const VIBuffer_Cube& prototype)
	: VIBuffer{ prototype }
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

	VTXCUBE* vertices = new VTXCUBE[_numVertices];
	vertices[0].position = XMFLOAT3(-0.5f, 0.5f, -0.5f);
	vertices[0].texcoord = vertices[0].position;
	vertices[1].position = XMFLOAT3(0.5f, 0.5f, -0.5f);
	vertices[1].texcoord = vertices[1].position;
	vertices[2].position = XMFLOAT3(0.5f, -0.5f, -0.5f);
	vertices[2].texcoord = vertices[2].position;
	vertices[3].position = XMFLOAT3(-0.5f,-0.5f, -0.5f);
	vertices[3].texcoord = vertices[3].position;
	vertices[4].position = XMFLOAT3(-0.5f, 0.5f, 0.5f);
	vertices[4].texcoord = vertices[4].position;
	vertices[5].position = XMFLOAT3(0.5f, 0.5f, 0.5f);
	vertices[5].texcoord = vertices[5].position;
	vertices[6].position = XMFLOAT3(0.5f, -0.5f, 0.5f);
	vertices[6].texcoord = vertices[6].position;
	vertices[7].position = XMFLOAT3(-0.5f, -0.5f, 0.5f);
	vertices[7].texcoord = vertices[7].position;

	D3D11_SUBRESOURCE_DATA VertexInitialData{};
	VertexInitialData.pSysMem = vertices;

	if (FAILED(_device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, _vb.GetAddressOf())))
		return E_FAIL;

	D3D11_BUFFER_DESC IndexBufferDesc{};
	IndexBufferDesc.ByteWidth = _indexStride * _numIndices;
	IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IndexBufferDesc.StructureByteStride = _indexStride;

	uint16* indices = new uint16[_numIndices];
	indices[0] = 1; indices[1] = 5; indices[2] = 6;
	indices[3] = 1; indices[4] = 6; indices[5] = 2;
	indices[6] = 4; indices[7] = 0; indices[8] = 3;
	indices[9] = 4; indices[10] = 3; indices[11] = 7;
	indices[12] = 4; indices[13] = 5; indices[14] = 1;
	indices[15] = 4; indices[16] = 1; indices[17] = 0;
	indices[18] = 3; indices[19] = 2; indices[20] = 6;
	indices[21] = 3; indices[22] = 6; indices[23] = 7;
	indices[24] = 7; indices[25] = 6; indices[26] = 5;
	indices[27] = 7; indices[28] = 5; indices[29] = 4;
	indices[30] = 0; indices[31] = 1; indices[32] = 2;
	indices[33] = 0; indices[34] = 2; indices[35] = 3;

	D3D11_SUBRESOURCE_DATA IndexInitialData{};
	IndexInitialData.pSysMem = indices;

	if (FAILED(_device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, _ib.GetAddressOf())))
		return E_FAIL;

	Safe_Delete_Array(vertices);
	Safe_Delete_Array(indices);

	return S_OK;
}

HRESULT VIBuffer_Cube::Initialize(void* arg)
{
	return S_OK;
}

Shared<VIBuffer_Cube> VIBuffer_Cube::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<VIBuffer_Cube>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : VIBuffer_Cube");
		return nullptr;
	}
	return instance;
}

Shared<Component> VIBuffer_Cube::Clone(void* arg)
{
	auto instance = make_shared<VIBuffer_Cube>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : VIBuffer_Cube");
		return nullptr;
	}
	return instance;
}

void VIBuffer_Cube::Free()
{
	__super::Free();
}
