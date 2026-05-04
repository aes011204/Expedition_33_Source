#include "Bone.h"

Bone::Bone()
{
}

HRESULT Bone::Initialize(const ModelBoneData& boneData)
{
	strcpy_s(_name, boneData.name);
	_parentBoneIndex = boneData.parentBoneIndex;
	_transformationMatrix = boneData.localTransform;
    _combinedTransformationMatrix = Matrix::Identity;

	return S_OK;
}

void Bone::Update_CombinedTransformationMatrix(const vector<Shared<Bone>>& bones, const Matrix& preTransformMatrix)
{
	if (-1 == _parentBoneIndex)
	{
        _combinedTransformationMatrix = _transformationMatrix * preTransformMatrix;
	}
	else
	{
        _combinedTransformationMatrix = _transformationMatrix * bones[_parentBoneIndex]->_combinedTransformationMatrix;
	}
}

void Bone::Recursive_Update_CombinedTransformationMatrix(const vector<Shared<Bone>>& bones, const Matrix& parentCombinedMatrix)
{
    _combinedTransformationMatrix = _transformationMatrix * parentCombinedMatrix;

	for (int32 childIndex : _children)
	{
		bones[childIndex]->Recursive_Update_CombinedTransformationMatrix(bones, _combinedTransformationMatrix));
	}
}

void Bone::Update_TransformationMatrix(const Matrix& transformMatrix)
{
	_transformationMatrix = transformMatrix;
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
	auto instance = make_shared<Bone>(*this);
	instance->_children.clear();
	return instance;
}

void Bone::Free()
{
	__super::Free();
}
