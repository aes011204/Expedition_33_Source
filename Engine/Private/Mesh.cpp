#include "Mesh.h"

#include "GameInstance.h"
#include "ModelCom.h"
#include "Bone.h"

Mesh::Mesh(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: VIBuffer { device, context }
{

}

Mesh::Mesh(const Mesh& prototype)
	: VIBuffer{ prototype }
{
}

HRESULT Mesh::Initialize_Prototype(ModelType type, const aiMesh* aiMesh, class ModelCom* model, FXMMATRIX preTransformMatrix)
{
	strcpy_s(_name, aiMesh->mName.data);

	_materialIndex = aiMesh->mMaterialIndex;

	_numVertexBuffers = 1;
	_numVertices = aiMesh->mNumVertices;	
	_numIndices = aiMesh->mNumFaces * 3;
	_indexStride = 4;
	_primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;


	/* Vertex Buffer */
	HRESULT		hr = ModelType::NonAnim == type ?
		Ready_VertexBuffer_For_NonAnim(aiMesh, preTransformMatrix) :
		Ready_VertexBuffer_For_Anim(aiMesh, model);

	if (FAILED(hr))
		return E_FAIL;

	/* Index Buffer */
	D3D11_BUFFER_DESC	IndexBufferDesc{};
	IndexBufferDesc.ByteWidth = _indexStride * _numIndices;
	IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IndexBufferDesc.StructureByteStride = _indexStride;
	IndexBufferDesc.CPUAccessFlags = 0;
	IndexBufferDesc.MiscFlags = 0;

	uint32	numIndices = {};

	uint32* indices = new uint32[_numIndices];

	for (size_t i = 0; i < aiMesh->mNumFaces; i++)
	{
		aiFace	AIFace = aiMesh->mFaces[i];

		indices[numIndices++] = AIFace.mIndices[0];
		indices[numIndices++] = AIFace.mIndices[1];
		indices[numIndices++] = AIFace.mIndices[2];
	}

	D3D11_SUBRESOURCE_DATA			IndexInitialData{};
	IndexInitialData.pSysMem = indices;

	if (FAILED(_device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, _ib.GetAddressOf())))
		return E_FAIL;


	Safe_Delete_Array(indices);

	return S_OK;
}

HRESULT Mesh::Initialize(void* arg)
{

	return S_OK;
}

HRESULT Mesh::Bind_BoneMatrices(ShaderCom* shader, const char* constantName, const vector<Shared<Bone>>& bones)
{
	ZeroMemory(_boneMatrices, sizeof(XMFLOAT4X4) * 512);

	for (uint32 i = 0; i < _numBones; i++)
	{
		XMStoreFloat4x4(&_boneMatrices[i], XMLoadFloat4x4(&_offsetMatrices[i]) * 
			XMLoadFloat4x4(bones[_boneIndices[i]]->Get_CombinedTransformationMatrixPtr()));
	}

	return shader->Bind_Matrices(constantName, _boneMatrices, _numBones);	
}

HRESULT Mesh::Ready_VertexBuffer_For_NonAnim(const aiMesh* aiMesh, FXMMATRIX preTransformMatrix)
{

	_vertexStride = sizeof(VTXMESH);
	D3D11_BUFFER_DESC	VertexBufferDesc{};
	VertexBufferDesc.ByteWidth = _vertexStride * _numVertices;
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VertexBufferDesc.StructureByteStride = _vertexStride;
	VertexBufferDesc.CPUAccessFlags = 0;
	VertexBufferDesc.MiscFlags = 0;

	VTXMESH* vertices = new VTXMESH[_numVertices];

	for (size_t i = 0; i < _numVertices; i++)
	{
		memcpy(&vertices[i].position, &aiMesh->mVertices[i], sizeof(XMFLOAT3));
		XMStoreFloat3(&vertices[i].position,
			XMVector3TransformCoord(XMLoadFloat3(&vertices[i].position), preTransformMatrix));

		memcpy(&vertices[i].normal, &aiMesh->mNormals[i], sizeof(XMFLOAT3));
		XMStoreFloat3(&vertices[i].normal,
			XMVector3TransformNormal(XMLoadFloat3(&vertices[i].normal), preTransformMatrix));

		memcpy(&vertices[i].tangent, &aiMesh->mTangents[i], sizeof(XMFLOAT3));
		XMStoreFloat3(&vertices[i].tangent,
			XMVector3TransformNormal(XMLoadFloat3(&vertices[i].tangent), preTransformMatrix));

		memcpy(&vertices[i].texcoord, &aiMesh->mTextureCoords[0][i], sizeof(XMFLOAT2));
	}


	D3D11_SUBRESOURCE_DATA			VertexInitialData{};
	VertexInitialData.pSysMem = vertices;

	if (FAILED(_device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, _vb.GetAddressOf())))
		return E_FAIL;

	Safe_Delete_Array(vertices);

	return S_OK;
}

HRESULT Mesh::Ready_VertexBuffer_For_Anim(const aiMesh* aiMesh, class ModelCom* model)
{
	_vertexStride = sizeof(VTXANIMMESH);
	D3D11_BUFFER_DESC	VertexBufferDesc{};
	VertexBufferDesc.ByteWidth = _vertexStride * _numVertices;
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VertexBufferDesc.StructureByteStride = _vertexStride;
	VertexBufferDesc.CPUAccessFlags = 0;
	VertexBufferDesc.MiscFlags = 0;

	VTXANIMMESH* vertices = new VTXANIMMESH[_numVertices];
	ZeroMemory(vertices, sizeof(VTXANIMMESH) * _numVertices);

	for (size_t i = 0; i < _numVertices; i++)
	{
		memcpy(&vertices[i].position, &aiMesh->mVertices[i], sizeof(XMFLOAT3));	
		memcpy(&vertices[i].normal, &aiMesh->mNormals[i], sizeof(XMFLOAT3));		
		memcpy(&vertices[i].tangent, &aiMesh->mTangents[i], sizeof(XMFLOAT3));		
		memcpy(&vertices[i].texcoord, &aiMesh->mTextureCoords[0][i], sizeof(XMFLOAT2));
	}

	_numBones = aiMesh->mNumBones;

	for (size_t i = 0; i < _numBones; i++)
	{
		aiBone* aiBone = aiMesh->mBones[i];

		int32		boneIndex = model->Get_BoneIndex(aiBone->mName.data);
		if (-1 == boneIndex)
			return E_FAIL;

		_boneIndices.push_back(boneIndex);

		XMFLOAT4X4		OffsetMatrix = {};

		memcpy(&OffsetMatrix, &aiBone->mOffsetMatrix, sizeof(XMFLOAT4X4));
		XMStoreFloat4x4(&OffsetMatrix, XMMatrixTranspose(XMLoadFloat4x4(&OffsetMatrix)));


		_offsetMatrices.push_back(OffsetMatrix);



		for (size_t j = 0; j < aiBone->mNumWeights; j++)
		{			
			aiVertexWeight	AIVertexWeight = aiBone->mWeights[j];

			if (0 == vertices[AIVertexWeight.mVertexId].blendWeight.x)
			{
				vertices[AIVertexWeight.mVertexId].blendIndex.x = static_cast<uint32>(i);
				vertices[AIVertexWeight.mVertexId].blendWeight.x = AIVertexWeight.mWeight;
			}

			else if (0 == vertices[AIVertexWeight.mVertexId].blendWeight.y)
			{
				vertices[AIVertexWeight.mVertexId].blendIndex.y = static_cast<uint32>(i);
				vertices[AIVertexWeight.mVertexId].blendWeight.y = AIVertexWeight.mWeight;
			}

			else if (0 == vertices[AIVertexWeight.mVertexId].blendWeight.z)
			{
				vertices[AIVertexWeight.mVertexId].blendIndex.z = static_cast<uint32>(i);
				vertices[AIVertexWeight.mVertexId].blendWeight.z = AIVertexWeight.mWeight;
			}

			else if (0 == vertices[AIVertexWeight.mVertexId].blendWeight.w)
			{
				vertices[AIVertexWeight.mVertexId].blendIndex.w = static_cast<uint32>(i);
				vertices[AIVertexWeight.mVertexId].blendWeight.w = AIVertexWeight.mWeight;
			}
		}		
	}

	if (0 == _numBones)
	{		
		_boneIndices.push_back(model->Get_BoneIndex(_name));

		_numVertexBuffers = 1;

		XMFLOAT4X4		OffsetMatrix = {};
		XMStoreFloat4x4(&OffsetMatrix, XMMatrixIdentity());

		_offsetMatrices.push_back(OffsetMatrix);

	}

	D3D11_SUBRESOURCE_DATA			VertexInitialData{};
	VertexInitialData.pSysMem = vertices;

	if (FAILED(_device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, _vb.GetAddressOf())))
		return E_FAIL;

	Safe_Delete_Array(vertices);


	return S_OK;
}



Shared<Mesh> Mesh::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, ModelType type, const aiMesh* aiMesh, class ModelCom* model, FXMMATRIX preTransformMatrix)
{
	auto instance = make_shared<Mesh>(device, context);

	if (FAILED(instance->Initialize_Prototype(type, aiMesh, model, preTransformMatrix)))
	{
		MSG_BOX("Failed to Created : Mesh");
		return nullptr;
	}
	return instance;
}

Shared<Component> Mesh::Clone(void* arg)
{
	return nullptr;
}

void Mesh::Free()
{
	__super::Free();
}

