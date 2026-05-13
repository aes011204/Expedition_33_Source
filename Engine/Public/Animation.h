#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Channel;
class Bone;

class Animation final : public Base
{
public:
	Animation() = default;
	Animation(const Animation& prototype);
	~Animation() override = default;

public:
	float   Get_Duration() const { return _duration; }
    float   Get_DurationSec() const;
    float   Get_CurrentTimeSec() const;

    void    Set_CurrentTimeSec(float timeSec);
    void    Evaluate_CurrentPose(const vector<Shared<Bone>>& bones, int32 rootNodeIndex, bool isLoop);

	float   Get_Progress() const { return (_duration > 0.f) ? _currentTrackPosition / _duration : 0.f; }
	void    Set_Progress(float progress);
	void    Reset();

	const RootMotionFrame& Get_TransformVelocity(int32 rootNodeIndex) const;
    const string& Get_AnimationName() const { return _animationName; }

public:
	HRESULT Initialize(const ModelAnimationData& animData, const vector<ModelChannelData>& channels);

	bool    Update_TransformationMatrices(float timeDelta, const vector<Shared<Bone>>& bones, bool isLoop, int32 rootNodeIndex);
	void    Blend_TransformMatrix(
        float currentTimeDelta,
        const Shared<Animation>& nextAnim,
        float blendRatio,
		const vector<Shared<Bone>>& bones,
        bool isLoop,
        int32 rootNodeIndex);

private:
    string                      _animationName;
	uint32						_numChannels{};
	vector<Shared<Channel>>		_channels;
	vector<uint32>				_currentKeyFrameIndices;
	float						_duration{};
	float						_tickPerSecond{};
	float						_currentTrackPosition{};

public:
	static Shared<Animation> Create(const ModelAnimationData& animData, const vector<ModelChannelData>& channels);
	Shared<Animation> Clone();
	void Free() override;
};

NS_END
