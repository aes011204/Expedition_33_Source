#include "Bone.h"

Bone::Bone()
{
}

HRESULT Bone::Initialize(const aiNode* aiNode, int32 parentBoneIndex)
{
	strcpy_s(_name, aiNode->mName.data);

	_parentBoneIndex = parentBoneIndex;

	memcpy(&_transformationMatrix, &aiNode->mTransformation, sizeof(XMFLOAT4X4));
	XMStoreFloat4x4(&_transformationMatrix, XMMatrixTranspose(XMLoadFloat4x4(&_transformationMatrix)));

	XMStoreFloat4x4(&_combinedTransformationMatrix, XMMatrixIdentity());

	return S_OK;
}

void Bone::Update_CombinedTransformationMatrix(const vector<Shared<Bone>>& bones, FXMMATRIX preTransformMatrix)
{
	if (-1 == _parentBoneIndex)
	{
		XMStoreFloat4x4(&_combinedTransformationMatrix,
			XMLoadFloat4x4(&_transformationMatrix) * preTransformMatrix);


	}
	else
	{
		XMStoreFloat4x4(&_combinedTransformationMatrix,
			XMLoadFloat4x4(&_transformationMatrix) * XMLoadFloat4x4(&bones[_parentBoneIndex]->_combinedTransformationMatrix));

	}

	

}

void Bone::Update_TransformationMatrix(FXMMATRIX transformMatrix)
{
	XMStoreFloat4x4(&_transformationMatrix, transformMatrix);
}

Shared<Bone> Bone::Create(const aiNode* aiNode, int32 parentBoneIndex)
{
	auto instance = make_shared<Bone>();

	if (FAILED(instance->Initialize(aiNode, parentBoneIndex)))
	{
		MSG_BOX("Failed to Created : Bone");
		return nullptr;
	}
	return instance;
}

Shared<Bone> Bone::Clone()
{
	return make_shared<Bone>(*this);
}

void Bone::Free()
{
	__super::Free();
}
