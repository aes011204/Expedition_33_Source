#pragma once

#include "Base.h"

/* 특정 애니메이션 내에서 사용되는 뼈의 상태행렬정보들을 가지는 객체 */

NS_BEGIN(Engine)

class Channel final : public Base
{
public:
	Channel();
    Channel(const Channel& rhs);
	~Channel() override = default;

public:
	int32 Get_BoneIndex() const { return _boneIndex; }
	const RootMotionFrame& Get_TransformVelocity() const { return _frameDelta; }

public:
	HRESULT Initialize(const ModelChannelData& channelData);

	void Update_TransformationMatrix(uint32& currentKeyFrameIndex, float currentTrackPosition, float duration,
		const vector<Shared<class Bone>>& bones, int32 rootNodeIndex, bool isLoop);

	void Get_ChannelTransform(float trackPosition, uint32& keyFrameIndex, float duration, bool isLoop, RootMotionFrame& outTransform);
	void Update_Velocity(const RootMotionFrame& curTransform, float trackPosition);
	void ResetVelocityState();
    Vec3 Get_TotalTranslation() const;
    Vec3 Get_FirstFramePosition() const;

    const vector<KEYFRAME>& Get_KeyFrames() const { return _keyFrames; }
    size_t Get_DebugEstimatedBytes() const;

private:
	vector<KEYFRAME>		_keyFrames;
	uint32					_numKeyFrames = {};
	int32					_boneIndex = { -1 };

	RootMotionFrame			_frameDelta{};
	Vec3					_prevPos{};
	Quat					_prevRot{ 0.f, 0.f, 0.f, 1.f };
	float					_prevTrackPosition{ -1.f };
	bool					_isFirstUpdate{ true };

public:
	static Shared<Channel> Create(const ModelChannelData& channelData);
	Shared<Channel> Clone();
	virtual void Free() override;
};

NS_END
