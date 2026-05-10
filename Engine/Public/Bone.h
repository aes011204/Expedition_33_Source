#pragma once

#include "Base.h"

/* 뼈하나의 정보. */
/* 부모 뼈의 정보와 현재 뼈의 상태행렬들을 가진다. */

NS_BEGIN(Engine)

class Bone final : public Base
{
public:
	Bone();
    Bone(const Bone& rhs);
	~Bone() override = default;

public:
        const Matrix* Get_OffsetMatrix() const { return &_offsetMatrix; }
        const Matrix* Get_CombinedTransformationMatrixPtr() const { return &_combinedTransformationMatrix; }
        bool Check_BoneByName(const string& boneName) const { return boneName == _name; }
        int32 Get_ParentIndex() const { return _parentBoneIndex; }
        const string& Get_BoneName() const { return _name; }

public:
        HRESULT Initialize(const ModelBoneData& boneData);
        void Update_CombinedTransformationMatrix(const vector<Shared<Bone>>& bones, const Matrix& preTransformMatrix);
        void Update_TransformationMatrix(const Matrix& transformMatrix);

        const Matrix& Get_TransformationMatrix() const { return _transformationMatrix; }
        const Matrix& Get_CombinedTransformationMatrix() const { return _combinedTransformationMatrix; }

private:
        string              _name;
        Matrix              _transformationMatrix{};
        Matrix              _combinedTransformationMatrix{};
        Matrix              _offsetMatrix{};
        int32               _parentBoneIndex = { -1 };

public:
	static Shared<Bone> Create(const ModelBoneData& boneData);
	Shared<Bone> Clone();
	virtual void Free() override;
};

NS_END
