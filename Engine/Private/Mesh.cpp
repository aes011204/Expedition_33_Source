#include "Mesh.h"

#include "GameInstance.h"
#include "Model.h"
#include "Bone.h"

Mesh::Mesh(ComPtr<Device> device, ComPtr<Context> context)
	: VIBuffer { device, context }
{

}

Mesh::Mesh(const Mesh& Prototype)
	: VIBuffer{ Prototype }
{
}

HRESULT Mesh::Initialize_Prototype(ModelType eType, const aiMesh* pAIMesh, class Model* pModel, FXMMATRIX PreTransformMatrix)
{
	strcpy_s(_name, pAIMesh->mName.data);

	_materialIndex = pAIMesh->mMaterialIndex;

	_numVertexBuffers = 1;
	_numVertices = pAIMesh->mNumVertices;	
	_numIndices = pAIMesh->mNumFaces * 3;
	_indexStride = 4;
	_primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;


	/* Vertex Buffer */
	HRESULT		hr = ModelType::NonAnim == eType ?
		Ready_VertexBuffer_For_NonAnim(pAIMesh, PreTransformMatrix) :
		Ready_VertexBuffer_For_Anim(pAIMesh, pModel);

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

	uint32	iNumIndices = {};

	uint32* pIndices = new uint32[_numIndices];

	for (size_t i = 0; i < pAIMesh->mNumFaces; i++)
	{
		aiFace	AIFace = pAIMesh->mFaces[i];

		pIndices[iNumIndices++] = AIFace.mIndices[0];
		pIndices[iNumIndices++] = AIFace.mIndices[1];
		pIndices[iNumIndices++] = AIFace.mIndices[2];
	}

	D3D11_SUBRESOURCE_DATA			IndexInitialData{};
	IndexInitialData.pSysMem = pIndices;

	if (FAILED(_device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, &_ib)))
		return E_FAIL;


	Safe_Delete_Array(pIndices);

	return S_OK;
}

HRESULT Mesh::Initialize(void* pArg)
{

	return S_OK;
}

HRESULT Mesh::Bind_BoneMatrices(Shader* pShader, const char* pConstantName, const vector<Shared<Bone>>& Bones)
{
	ZeroMemory(_boneMatrices, sizeof(XMFLOAT4X4) * 512);

	for (size_t i = 0; i < _numBones; i++)
	{
		XMStoreFloat4x4(&_boneMatrices[i], XMLoadFloat4x4(&_offsetMatrices[i]) * 
			XMLoadFloat4x4(Bones[_boneIndices[i]]->Get_CombinedTransformationMatrixPtr()));
	}

	return pShader->Bind_Matrices(pConstantName, _boneMatrices, _numBones);	
}

HRESULT Mesh::Ready_VertexBuffer_For_NonAnim(const aiMesh* pAIMesh, FXMMATRIX PreTransformMatrix)
{

	_vertexStride = sizeof(VTXMESH);
	D3D11_BUFFER_DESC	VertexBufferDesc{};
	VertexBufferDesc.ByteWidth = _vertexStride * _numVertices;
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VertexBufferDesc.StructureByteStride = _vertexStride;
	VertexBufferDesc.CPUAccessFlags = 0;
	VertexBufferDesc.MiscFlags = 0;

	VTXMESH* pVertices = new VTXMESH[_numVertices];

	for (size_t i = 0; i < _numVertices; i++)
	{
		memcpy(&pVertices[i].vPosition, &pAIMesh->mVertices[i], sizeof(XMFLOAT3));
		XMStoreFloat3(&pVertices[i].vPosition,
			XMVector3TransformCoord(XMLoadFloat3(&pVertices[i].vPosition), PreTransformMatrix));

		memcpy(&pVertices[i].vNormal, &pAIMesh->mNormals[i], sizeof(XMFLOAT3));
		XMStoreFloat3(&pVertices[i].vNormal,
			XMVector3TransformNormal(XMLoadFloat3(&pVertices[i].vNormal), PreTransformMatrix));

		memcpy(&pVertices[i].vTangent, &pAIMesh->mTangents[i], sizeof(XMFLOAT3));
		XMStoreFloat3(&pVertices[i].vTangent,
			XMVector3TransformNormal(XMLoadFloat3(&pVertices[i].vTangent), PreTransformMatrix));

		memcpy(&pVertices[i].vTexcoord, &pAIMesh->mTextureCoords[0][i], sizeof(XMFLOAT2));
	}


	D3D11_SUBRESOURCE_DATA			VertexInitialData{};
	VertexInitialData.pSysMem = pVertices;

	if (FAILED(_device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, &_vb)))
		return E_FAIL;

	Safe_Delete_Array(pVertices);

	return S_OK;
}

HRESULT Mesh::Ready_VertexBuffer_For_Anim(const aiMesh* pAIMesh, class Model* pModel)
{
	_vertexStride = sizeof(VTXANIMMESH);
	D3D11_BUFFER_DESC	VertexBufferDesc{};
	VertexBufferDesc.ByteWidth = _vertexStride * _numVertices;
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VertexBufferDesc.StructureByteStride = _vertexStride;
	VertexBufferDesc.CPUAccessFlags = 0;
	VertexBufferDesc.MiscFlags = 0;

	VTXANIMMESH* pVertices = new VTXANIMMESH[_numVertices];
	ZeroMemory(pVertices, sizeof(VTXANIMMESH) * _numVertices);

	for (size_t i = 0; i < _numVertices; i++)
	{
		memcpy(&pVertices[i].vPosition, &pAIMesh->mVertices[i], sizeof(XMFLOAT3));	
		memcpy(&pVertices[i].vNormal, &pAIMesh->mNormals[i], sizeof(XMFLOAT3));		
		memcpy(&pVertices[i].vTangent, &pAIMesh->mTangents[i], sizeof(XMFLOAT3));		
		memcpy(&pVertices[i].vTexcoord, &pAIMesh->mTextureCoords[0][i], sizeof(XMFLOAT2));
	}

	_numBones = pAIMesh->mNumBones;

	for (size_t i = 0; i < _numBones; i++)
	{
		aiBone* pAIBone = pAIMesh->mBones[i];

		int32		iBoneIndex = pModel->Get_BoneIndex(pAIBone->mName.data);
		if (-1 == iBoneIndex)
			return E_FAIL;

		_boneIndices.push_back(iBoneIndex);

		XMFLOAT4X4		OffsetMatrix = {};

		memcpy(&OffsetMatrix, &pAIBone->mOffsetMatrix, sizeof(XMFLOAT4X4));
		XMStoreFloat4x4(&OffsetMatrix, XMMatrixTranspose(XMLoadFloat4x4(&OffsetMatrix)));


		_offsetMatrices.push_back(OffsetMatrix);



		for (size_t j = 0; j < pAIBone->mNumWeights; j++)
		{			
			aiVertexWeight	AIVertexWeight = pAIBone->mWeights[j];

			if (0 == pVertices[AIVertexWeight.mVertexId].vBlendWeight.x)
			{
				pVertices[AIVertexWeight.mVertexId].vBlendIndex.x = i;
				pVertices[AIVertexWeight.mVertexId].vBlendWeight.x = AIVertexWeight.mWeight;
			}

			else if (0 == pVertices[AIVertexWeight.mVertexId].vBlendWeight.y)
			{
				pVertices[AIVertexWeight.mVertexId].vBlendIndex.y = i;
				pVertices[AIVertexWeight.mVertexId].vBlendWeight.y = AIVertexWeight.mWeight;
			}

			else if (0 == pVertices[AIVertexWeight.mVertexId].vBlendWeight.z)
			{
				pVertices[AIVertexWeight.mVertexId].vBlendIndex.z = i;
				pVertices[AIVertexWeight.mVertexId].vBlendWeight.z = AIVertexWeight.mWeight;
			}

			else if (0 == pVertices[AIVertexWeight.mVertexId].vBlendWeight.w)
			{
				pVertices[AIVertexWeight.mVertexId].vBlendIndex.w = i;
				pVertices[AIVertexWeight.mVertexId].vBlendWeight.w = AIVertexWeight.mWeight;
			}
		}		
	}

	if (0 == _numBones)
	{		
		_boneIndices.push_back(pModel->Get_BoneIndex(_name));

		_numVertexBuffers = 1;

		XMFLOAT4X4		OffsetMatrix = {};
		XMStoreFloat4x4(&OffsetMatrix, XMMatrixIdentity());

		_offsetMatrices.push_back(OffsetMatrix);

	}

	D3D11_SUBRESOURCE_DATA			VertexInitialData{};
	VertexInitialData.pSysMem = pVertices;

	if (FAILED(_device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, &_vb)))
		return E_FAIL;

	Safe_Delete_Array(pVertices);


	return S_OK;
}



Shared<Mesh> Mesh::Create(ComPtr<Device> device, ComPtr<Context> context, ModelType eType, const aiMesh* pAIMesh, class Model* pModel, FXMMATRIX PreTransformMatrix)
{
	auto pInstance = make_shared<Mesh>(device, context);

	if (FAILED(pInstance->Initialize_Prototype(eType, pAIMesh, pModel, PreTransformMatrix)))
	{
		MSG_BOX("Failed to Created : Mesh");
		return nullptr;
	}
	return pInstance;
}

Shared<Component> Mesh::Clone(void* pArg)
{
	return nullptr;
}

void Mesh::Free()
{
	__super::Free();
}

