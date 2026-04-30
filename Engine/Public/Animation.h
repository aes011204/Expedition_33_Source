#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Animation final : public Base
{
private:
	Animation();
	Animation(const Animation& Prototype);
	virtual ~Animation() = default;

public:
	HRESULT Initialize(const aiAnimation* pAIAnimation, class Model* pModel);
	bool Update_TransformationMatrices(float fTimeDelta, const vector<Shared<class Bone>>& Bones, bool isLoop);

private:
	float		_duration = {};
	float		_tickPerSecond = {};

	float		_currentTrackPosition = {};

	uint32		_numChannels = {};
	vector<Shared<class Channel>>	_channels;
	vector<uint32>	_currentKeyFrameIndices;


public:
	static Shared<Animation> Create(const aiAnimation* pAIAnimation, class Model* pModel);
	Shared<Animation> Clone();
	virtual void Free() override;
};

NS_END