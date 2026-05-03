#pragma once

#include "Base.h"

/* 특정 애니메이션 내에서 사용되는 뼈의 상태행렬정보들을 가지는 객체 */

NS_BEGIN(Engine)

class Channel final : public Base
{
public:
	Channel();
	virtual ~Channel() = default;

public:
	HRESULT Initialize(const aiNodeAnim* aiChannel, class ModelCom* model);
	void Update_TransformationMatrix(uint32* currentKeyFrameIndex, float currentTrackPosition, const vector<Shared<class Bone>>& bones);

private:
	vector<KEYFRAME>			_keyFrames;
	uint32						_numKeyFrames = {};

	int32						_boneIndex = { -1 };

public:
	static Shared<Channel> Create(const aiNodeAnim* aiChannel, class ModelCom* model);
	virtual void Free() override;
};

NS_END
