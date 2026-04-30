#include "Bone.h"

Bone::Bone()
{
}

HRESULT Bone::Initialize(const aiNode* pAINode, int32 iParentBoneIndex)
{
	strcpy_s(_name, pAINode->mName.data);

	_parentBoneIndex = iParentBoneIndex;

	memcpy(&_transformationMatrix, &pAINode->mTransformation, sizeof(XMFLOAT4X4));
	XMStoreFloat4x4(&_transformationMatrix, XMMatrixTranspose(XMLoadFloat4x4(&_transformationMatrix)));

	XMStoreFloat4x4(&_combinedTransformationMatrix, XMMatrixIdentity());

	return S_OK;
}

void Bone::Update_CombinedTransformationMatrix(const vector<Shared<Bone>>& Bones, FXMMATRIX PreTransformMatrix)
{
	if (-1 == _parentBoneIndex)
	{
		XMStoreFloat4x4(&_combinedTransformationMatrix,
			XMLoadFloat4x4(&_transformationMatrix) * PreTransformMatrix);


	}
	else
	{
		XMStoreFloat4x4(&_combinedTransformationMatrix,
			XMLoadFloat4x4(&_transformationMatrix) * XMLoadFloat4x4(&Bones[_parentBoneIndex]->_combinedTransformationMatrix));

	}

	

}

void Bone::Update_TransformationMatrix(FXMMATRIX TransformMatrix)
{
	XMStoreFloat4x4(&_transformationMatrix, TransformMatrix);
}

Shared<Bone> Bone::Create(const aiNode* pAINode, int32 iParentBoneIndex)
{
	Shared<Bone> pInstance = MakeSharedFromRaw(new Bone());

	if (FAILED(pInstance->Initialize(pAINode, iParentBoneIndex)))
	{
		MSG_BOX("Failed to Created : Bone");
		Safe_Release(pInstance);
	}
	return pInstance;
}

Shared<Bone> Bone::Clone()
{
	return MakeSharedFromRaw(new Bone(*this));
}

void Bone::Free()
{
	__super::Free();
}
