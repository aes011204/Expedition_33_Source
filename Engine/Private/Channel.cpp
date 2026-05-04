#include "Channel.h"

#include "Bone.h"

Channel::Channel()
{
}

HRESULT Channel::Initialize(const ModelChannelData& channelData)
{
	_boneIndex = channelData.boneIndex;
	_numKeyFrames = channelData.numKeyframes;

	KEYFRAME* pKeyframes = static_cast<KEYFRAME*>(channelData.keyframes);
	_keyFrames.assign(pKeyframes, pKeyframes + _numKeyFrames);

	return S_OK;
}

void Channel::Get_ChannelTransform(float trackPosition, uint32& keyFrameIndex, float duration, bool isLoop, RootMotionFrame& outTransform)
{
	if (trackPosition < _keyFrames[keyFrameIndex].trackPosition)
		keyFrameIndex = 0;

	KEYFRAME lastKeyFrame = _keyFrames.back();

	Vec3 scale, translation;
    Quat rotation;

	if (trackPosition >= lastKeyFrame.trackPosition)
	{
		scale = lastKeyFrame.scale;
		rotation = lastKeyFrame.rotation;
		translation = lastKeyFrame.translation;
	}
	else
	{
		while (keyFrameIndex + 1 < _numKeyFrames && trackPosition >= _keyFrames[keyFrameIndex + 1].trackPosition)
			++keyFrameIndex;

		float ratio = (trackPosition - _keyFrames[keyFrameIndex].trackPosition) /
			(_keyFrames[keyFrameIndex + 1].trackPosition - _keyFrames[keyFrameIndex].trackPosition);

        scale = Vec3::Lerp(_keyFrames[keyFrameIndex].scale, _keyFrames[keyFrameIndex + 1].scale, ratio);
		rotation = Quat::Slerp(_keyFrames[keyFrameIndex].rotation, _keyFrames[keyFrameIndex + 1].rotation, ratio);
        translation = Vec3::Lerp(_keyFrames[keyFrameIndex].translation, _keyFrames[keyFrameIndex + 1].translation, ratio);
	}

	XMStoreFloat3(&outTransform.scale, scale);
	XMStoreFloat4(&outTransform.rotation, rotation);
	XMStoreFloat3(&outTransform.position, translation);
}

void Channel::Update_Velocity(const RootMotionFrame& curTransform, float trackPosition)
{
	if (_isFirstUpdate || trackPosition < _prevTrackPosition)
	{
		_prevPos = curTransform.position;
		_prevRot = curTransform.rotation;
		_isFirstUpdate = false;
	}

	_frameDelta.position = curTransform.position - _prevPos;

	Quat invPrevRot = _prevRot;
	invPrevRot.Conjugate();
	_frameDelta.rotation = curTransform.rotation * invPrevRot;

	_prevPos = curTransform.position;
	_prevRot = curTransform.rotation;
	_prevTrackPosition = trackPosition;
}

void Channel::ResetVelocityState()
{
	_isFirstUpdate = true;
	_prevTrackPosition = -1.f;
}

void Channel::Update_TransformationMatrix(uint32& currentKeyFrameIndex, float currentTrackPosition, float duration,
	const vector<Shared<Bone>>& bones, int32 rootNodeIndex, bool isLoop)
{
	if (_boneIndex == -1 || _boneIndex >= bones.size())
		return;

	RootMotionFrame transform{};
	Get_ChannelTransform(currentTrackPosition, currentKeyFrameIndex, duration, isLoop, transform);
	Update_Velocity(transform, currentTrackPosition);

	XMMATRIX targetMatrix;

	if (_boneIndex == rootNodeIndex)
	{
		/* 루트 본의 Translation을 0으로 강제하여 제자리 유지 */
		targetMatrix = XMMatrixAffineTransformation(
            transform.scale,
            XMVectorZero(),
            transform.rotation,
            Vec3::Zero
        );
	}
	else
	{
		targetMatrix = XMMatrixAffineTransformation(
            transform.scale,
            XMVectorZero(),
            transform.rotation,
            transform.position
        );
	}

	bones[_boneIndex]->Update_TransformationMatrix(targetMatrix);
}

Shared<Channel> Channel::Create(const ModelChannelData& channelData)
{
	auto instance = make_shared<Channel>();

	if (FAILED(instance->Initialize(channelData)))
	{
		MSG_BOX("Failed to Created : Channel");
		return nullptr;
	}
	return instance;
}

Shared<Channel> Channel::Clone()
{
	return make_shared<Channel>(*this);
}

void Channel::Free()
{
	__super::Free();

	_keyFrames.clear();
}
