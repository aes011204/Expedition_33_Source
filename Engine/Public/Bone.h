#pragma once

#include "Base.h"

/* 뼈하나의 정보. */
/* 부모 뼈의 정보와 현재 뼈의 상태행렬들을 가진다. */
/* assimp 내에서 이용되는 뼈 : aiNode, aiBone, aiNodeAnim */

NS_BEGIN(Engine)

class Bone final : public Base
{
private:
	Bone();
	virtual ~Bone() = default;
public:
	const XMFLOAT4X4* Get_CombinedTransformationMatrixPtr() const {
		return &_combinedTransformationMatrix;
	}
public:
	HRESULT Initialize(const aiNode* pAINode, int32 iParentBoneIndex);
	bool isCompare(const char* pBoneName) {
		return !strcmp(pBoneName, _name);
	}

	void Update_CombinedTransformationMatrix(const vector<Shared<Bone>>& Bones, FXMMATRIX PreTransformMatrix);
	void Update_TransformationMatrix(FXMMATRIX TransformMatrix);



private:
	char					_name[MAX_PATH] = {};
	XMFLOAT4X4				_transformationMatrix = {};
	XMFLOAT4X4				_combinedTransformationMatrix = {};	
	int32					_parentBoneIndex = { -1 };

public:
	static Shared<Bone> Create(const aiNode* pAINode, int32 iParentBoneIndex);
	Shared<Bone> Clone();
	virtual void Free() override;
};

NS_END