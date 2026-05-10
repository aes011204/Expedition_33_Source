#include "Bone.h"

Bone::Bone()
{
}

Bone::Bone(const Bone& rhs)
    : _parentBoneIndex{rhs._parentBoneIndex}
{
    _name = rhs._name;
    _transformationMatrix = rhs._transformationMatrix;
    _combinedTransformationMatrix = rhs._combinedTransformationMatrix;
}

HRESULT Bone::Initialize(const ModelBoneData& boneData)
{
	_name = boneData.name;
	_parentBoneIndex = boneData.parentBoneIndex;
	_transformationMatrix = boneData.localTransform;
    _combinedTransformationMatrix = Matrix::Identity;
    _offsetMatrix = boneData.offsetMatrix;

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
	return instance;
}

void Bone::Free()
{
	__super::Free();
}
