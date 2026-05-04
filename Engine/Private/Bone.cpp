#include "Bone.h"

Bone::Bone()
{
}

HRESULT Bone::Initialize(const ModelBoneData& boneData)
{
	strcpy_s(_name, boneData.name);
	_parentBoneIndex = boneData.nodeIndex;
	_transformationMatrix = boneData.offsetMatrix;
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

Shared<Bone> Bone::Create(const ModelBoneData& boneData)
{
	auto instance = make_shared<Bone>();

	if (FAILED(instance->Initialize(boneData)))
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
