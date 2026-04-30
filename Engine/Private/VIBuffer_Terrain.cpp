#include "VIBuffer_Terrain.h"

VIBuffer_Terrain::VIBuffer_Terrain(ComPtr<Device> device, ComPtr<Context> context)
	: VIBuffer { device, context }
{
}

VIBuffer_Terrain::VIBuffer_Terrain(const VIBuffer_Terrain& Prototype)
	: VIBuffer{ Prototype }
	, _numVerticesX { Prototype._numVerticesX }
	, _numVerticesZ { Prototype._numVerticesZ }
{
}

HRESULT VIBuffer_Terrain::Initialize_Prototype(const wchar_t* pHeightMapFilePath)
{
	unsigned long dwByte = {};
	HANDLE hFile = CreateFile(pHeightMapFilePath, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (INVALID_HANDLE_VALUE == hFile)
		return E_FAIL;

	BITMAPFILEHEADER fh{};
	ReadFile(hFile, &fh, sizeof fh, &dwByte, nullptr);

	BITMAPINFOHEADER ih{};
	ReadFile(hFile, &ih, sizeof ih, &dwByte, nullptr);

	_numVerticesX = ih.biWidth;
	_numVerticesZ = ih.biHeight;
	_numVertices = _numVerticesX * _numVerticesZ;

	uint32* pPixels = new uint32[_numVertices];
	ReadFile(hFile, pPixels, sizeof(uint32) * _numVertices, &dwByte, nullptr);

	_numVertexBuffers = 1;
	_vertexStride = sizeof(VTXNORTEX);
	_numIndices = (_numVerticesX - 1) * (_numVerticesZ - 1) * 2 * 3;
	_indexStride = 4;
	_primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	D3D11_BUFFER_DESC VertexBufferDesc{};
	VertexBufferDesc.ByteWidth = _vertexStride * _numVertices;
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VertexBufferDesc.StructureByteStride = _vertexStride;

	VTXNORTEX* pVertices = new VTXNORTEX[_numVertices];

	for (size_t i = 0; i < _numVerticesZ; i++)
	{
		for (size_t j = 0; j < _numVerticesX; j++)
		{
			size_t iIndex = i * _numVerticesX + j;
			pVertices[iIndex].vPosition = XMFLOAT3(static_cast<float>(j), (pPixels[iIndex] & 0x000000ff) / 10.f, static_cast<float>(i));
			pVertices[iIndex].vNormal = XMFLOAT3(0.f, 0.f, 0.f);
			pVertices[iIndex].vTexcoord = XMFLOAT2(j / (_numVerticesX - 1.f), i / (_numVerticesZ - 1.f));
		}
	}

	D3D11_BUFFER_DESC IndexBufferDesc{};
	IndexBufferDesc.ByteWidth = _indexStride * _numIndices;
	IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IndexBufferDesc.StructureByteStride = _indexStride;

	uint32* pIndices = new uint32[_numIndices];
	uint32 iNumIndices = 0;

	for (size_t i = 0; i < _numVerticesZ - 1; i++)
	{
		for (size_t j = 0; j < _numVerticesX - 1; j++)
		{
			size_t iIndex = i * _numVerticesX + j;

			uint32 iIndices[4] = {
				static_cast<uint32>(iIndex + _numVerticesX),
				static_cast<uint32>(iIndex + _numVerticesX + 1),
				static_cast<uint32>(iIndex + 1),
				static_cast<uint32>(iIndex)
			};

			XMVECTOR vSour, vDest, vNormal;

			pIndices[iNumIndices++] = iIndices[0];
			pIndices[iNumIndices++] = iIndices[1];
			pIndices[iNumIndices++] = iIndices[2];

			vSour = XMLoadFloat3(&pVertices[iIndices[1]].vPosition) - XMLoadFloat3(&pVertices[iIndices[0]].vPosition);
			vDest = XMLoadFloat3(&pVertices[iIndices[2]].vPosition) - XMLoadFloat3(&pVertices[iIndices[1]].vPosition);
			vNormal = XMVector3Normalize(XMVector3Cross(vSour, vDest));

			XMStoreFloat3(&pVertices[iIndices[0]].vNormal, XMLoadFloat3(&pVertices[iIndices[0]].vNormal) + vNormal);
			XMStoreFloat3(&pVertices[iIndices[1]].vNormal, XMLoadFloat3(&pVertices[iIndices[1]].vNormal) + vNormal);
			XMStoreFloat3(&pVertices[iIndices[2]].vNormal, XMLoadFloat3(&pVertices[iIndices[2]].vNormal) + vNormal);

			pIndices[iNumIndices++] = iIndices[0];
			pIndices[iNumIndices++] = iIndices[2];
			pIndices[iNumIndices++] = iIndices[3];

			vSour = XMLoadFloat3(&pVertices[iIndices[2]].vPosition) - XMLoadFloat3(&pVertices[iIndices[0]].vPosition);
			vDest = XMLoadFloat3(&pVertices[iIndices[3]].vPosition) - XMLoadFloat3(&pVertices[iIndices[2]].vPosition);
			vNormal = XMVector3Normalize(XMVector3Cross(vSour, vDest));

			XMStoreFloat3(&pVertices[iIndices[0]].vNormal, XMLoadFloat3(&pVertices[iIndices[0]].vNormal) + vNormal);
			XMStoreFloat3(&pVertices[iIndices[2]].vNormal, XMLoadFloat3(&pVertices[iIndices[2]].vNormal) + vNormal);
			XMStoreFloat3(&pVertices[iIndices[3]].vNormal, XMLoadFloat3(&pVertices[iIndices[3]].vNormal) + vNormal);
		}
	}

	for (size_t i = 0; i < _numVertices; i++)
		XMStoreFloat3(&pVertices[i].vNormal, XMVector3Normalize(XMLoadFloat3(&pVertices[i].vNormal)));

	D3D11_SUBRESOURCE_DATA VertexInitialData{};
	VertexInitialData.pSysMem = pVertices;

	if (FAILED(_device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, &_vb)))
		return E_FAIL;

	D3D11_SUBRESOURCE_DATA IndexInitialData{};
	IndexInitialData.pSysMem = pIndices;

	if (FAILED(_device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, &_ib)))
		return E_FAIL;

	Safe_Delete_Array(pVertices);
	Safe_Delete_Array(pIndices);
	Safe_Delete_Array(pPixels);
	CloseHandle(hFile);

	return S_OK;
}

HRESULT VIBuffer_Terrain::Initialize(void* pArg)
{
	return S_OK;
}

Shared<VIBuffer_Terrain> VIBuffer_Terrain::Create(ComPtr<Device> device, ComPtr<Context> context, const wchar_t* pHeightMapFilePath)
{
	Shared<VIBuffer_Terrain> pInstance = MakeSharedFromRaw(new VIBuffer_Terrain(device, context));

	if (FAILED(pInstance->Initialize_Prototype(pHeightMapFilePath)))
	{
		MSG_BOX("Failed to Created : VIBuffer_Terrain");
		Safe_Release(pInstance);
	}
	return pInstance;
}

Shared<Component> VIBuffer_Terrain::Clone(void* pArg)
{
	Shared<VIBuffer_Terrain> pInstance = MakeSharedFromRaw(new VIBuffer_Terrain(*this));

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : VIBuffer_Terrain");
		Safe_Release(pInstance);
	}
	return pInstance;
}

void VIBuffer_Terrain::Free()
{
	__super::Free();
}
