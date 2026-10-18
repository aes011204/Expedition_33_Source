#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Channel;
class Bone;

class ENGINE_DLL Animation final : public Base
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
    void    Set_StartTimeSec(float startSec);
    void    Evaluate_CurrentPose(const vector<Shared<Bone>>& bones, int32 rootNodeIndex, bool isLoop);

    float   Get_Progress() const;
	void    Set_Progress(float progress);

    void    Set_StartFrame(float startFrame);
	void    Reset();

    float   Get_AnimationSpeed() const { return _animationSpeed; }
    void    Set_AnimationSpeed(float speed) { _animationSpeed = speed; }

	const RootMotionFrame& Get_TransformVelocity(int32 rootNodeIndex) const;
    const string& Get_AnimationName() const { return _animationName; }
    Vec3 Get_RootTranslation(int32 rootNodeIndex) const;
    Vec3 Get_CurrentRootTranslation(int32 rootNodeIndex) const;

    const vector<Shared<Channel>>& Get_Channels() const { return _channels; }
    float Get_TickPerSecond() const { return _tickPerSecond; }
    size_t Get_DebugEstimatedBytes() const;

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
    void Build_ChannelIndexCache();
    int32 Find_ChannelIndexByBoneIndex(int32 boneIndex) const;

private:
    string                      _animationName;
	uint32						_numChannels{};
	vector<Shared<Channel>>		_channels;
	vector<uint32>				_currentKeyFrameIndices;
	float						_duration{};
	float						_tickPerSecond{};
	float						_currentTrackPosition{};
    float                       _animationSpeed{ 1.f };

    vector<int32>               _channelIndexByBoneIndex{};

public:
	static Shared<Animation> Create(const ModelAnimationData& animData, const vector<ModelChannelData>& channels);
	Shared<Animation> Clone();
	void Free() override;
};

NS_END
