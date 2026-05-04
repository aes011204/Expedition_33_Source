#include "VIBuffer_Terrain.h"

VIBuffer_Terrain::VIBuffer_Terrain(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: VIBuffer { device, context }
{
}

VIBuffer_Terrain::VIBuffer_Terrain(const VIBuffer_Terrain& prototype)
	: VIBuffer{ prototype }
	, _numVerticesX { prototype._numVerticesX }
	, _numVerticesZ { prototype._numVerticesZ }
{
}

HRESULT VIBuffer_Terrain::Initialize_Prototype(const wchar_t* heightMapFilePath)
{
	unsigned long byteCount = {};
	HANDLE hFile = CreateFile(heightMapFilePath, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (INVALID_HANDLE_VALUE == hFile)
		return E_FAIL;

	BITMAPFILEHEADER fh{};
	ReadFile(hFile, &fh, sizeof fh, &byteCount, nullptr);

	BITMAPINFOHEADER ih{};
	ReadFile(hFile, &ih, sizeof ih, &byteCount, nullptr);

	_numVerticesX = ih.biWidth;
	_numVerticesZ = ih.biHeight;
	_numVertices = _numVerticesX * _numVerticesZ;

	uint32* pixels = new uint32[_numVertices];
	ReadFile(hFile, pixels, sizeof(uint32) * _numVertices, &byteCount, nullptr);

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

	VTXNORTEX* vertices = new VTXNORTEX[_numVertices];

	for (size_t i = 0; i < _numVerticesZ; i++)
	{
		for (size_t j = 0; j < _numVerticesX; j++)
		{
			size_t index = i * _numVerticesX + j;
			vertices[index].position = XMFLOAT3(static_cast<float>(j), (pixels[index] & 0x000000ff) / 10.f, static_cast<float>(i));
			vertices[index].normal = XMFLOAT3(0.f, 0.f, 0.f);
			vertices[index].texcoord = XMFLOAT2(j / (_numVerticesX - 1.f), i / (_numVerticesZ - 1.f));
		}
	}

	D3D11_BUFFER_DESC IndexBufferDesc{};
	IndexBufferDesc.ByteWidth = _indexStride * _numIndices;
	IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IndexBufferDesc.StructureByteStride = _indexStride;

	uint32* terrainIndices = new uint32[_numIndices];
	uint32 numIndices = 0;

	for (size_t i = 0; i < _numVerticesZ - 1; i++)
	{
		for (size_t j = 0; j < _numVerticesX - 1; j++)
		{
			size_t index = i * _numVerticesX + j;

			uint32 quadIndices[4] = {
				static_cast<uint32>(index + _numVerticesX),
				static_cast<uint32>(index + _numVerticesX + 1),
				static_cast<uint32>(index + 1),
				static_cast<uint32>(index)
			};

			XMVECTOR sour, dest, normal;

			terrainIndices[numIndices++] = quadIndices[0];
			terrainIndices[numIndices++] = quadIndices[1];
			terrainIndices[numIndices++] = quadIndices[2];

			sour = XMLoadFloat3(&vertices[quadIndices[1]].position) - XMLoadFloat3(&vertices[quadIndices[0]].position);
			dest = XMLoadFloat3(&vertices[quadIndices[2]].position) - XMLoadFloat3(&vertices[quadIndices[1]].position);
			normal = XMVector3Normalize(XMVector3Cross(sour, dest));

			XMStoreFloat3(&vertices[quadIndices[0]].normal, XMLoadFloat3(&vertices[quadIndices[0]].normal) + normal);
			XMStoreFloat3(&vertices[quadIndices[1]].normal, XMLoadFloat3(&vertices[quadIndices[1]].normal) + normal);
			XMStoreFloat3(&vertices[quadIndices[2]].normal, XMLoadFloat3(&vertices[quadIndices[2]].normal) + normal);

			terrainIndices[numIndices++] = quadIndices[0];
			terrainIndices[numIndices++] = quadIndices[2];
			terrainIndices[numIndices++] = quadIndices[3];

			sour = XMLoadFloat3(&vertices[quadIndices[2]].position) - XMLoadFloat3(&vertices[quadIndices[0]].position);
			dest = XMLoadFloat3(&vertices[quadIndices[3]].position) - XMLoadFloat3(&vertices[quadIndices[2]].position);
			normal = XMVector3Normalize(XMVector3Cross(sour, dest));

			XMStoreFloat3(&vertices[quadIndices[0]].normal, XMLoadFloat3(&vertices[quadIndices[0]].normal) + normal);
			XMStoreFloat3(&vertices[quadIndices[2]].normal, XMLoadFloat3(&vertices[quadIndices[2]].normal) + normal);
			XMStoreFloat3(&vertices[quadIndices[3]].normal, XMLoadFloat3(&vertices[quadIndices[3]].normal) + normal);
		}
	}

	for (size_t i = 0; i < _numVertices; i++)
		XMStoreFloat3(&vertices[i].normal, XMVector3Normalize(XMLoadFloat3(&vertices[i].normal)));

	D3D11_SUBRESOURCE_DATA VertexInitialData{};
	VertexInitialData.pSysMem = vertices;

	if (FAILED(_device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, _vb.GetAddressOf())))
		return E_FAIL;

	D3D11_SUBRESOURCE_DATA IndexInitialData{};
	IndexInitialData.pSysMem = terrainIndices;

	if (FAILED(_device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, _ib.GetAddressOf())))
		return E_FAIL;

	Safe_Delete_Array(vertices);
	Safe_Delete_Array(terrainIndices);
	Safe_Delete_Array(pixels);
	CloseHandle(hFile);

	return S_OK;
}

HRESULT VIBuffer_Terrain::Initialize(void* arg)
{
	return S_OK;
}

Shared<VIBuffer_Terrain> VIBuffer_Terrain::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const wchar_t* heightMapFilePath)
{
	auto instance = make_shared<VIBuffer_Terrain>(device, context);

	if (FAILED(instance->Initialize_Prototype(heightMapFilePath)))
	{
		LOG_WARN("Failed to create VIBuffer_Terrain: path='{}'", String::ToString(heightMapFilePath));
		return nullptr;
	}
	return instance;
}

Shared<Component> VIBuffer_Terrain::Clone(void* arg)
{
	auto instance = make_shared<VIBuffer_Terrain>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : VIBuffer_Terrain");
		return nullptr;
	}
	return instance;
}

void VIBuffer_Terrain::Free()
{
	__super::Free();
}
