#include "Channel.h"

#include "Bone.h"

Channel::Channel()
{
}

Channel::Channel(const Channel& rhs)
    : _keyFrames{ rhs._keyFrames }, _numKeyFrames{rhs._numKeyFrames}, _boneIndex{ rhs._boneIndex }
{
    _isFirstUpdate = true;
}

HRESULT Channel::Initialize(const ModelChannelData& channelData)
{
	_boneIndex = channelData.boneIndex;
	_numKeyFrames = channelData.numKeyframes;

	KEYFRAME* pKeyframes = static_cast<KEYFRAME*>(channelData.keyframes);
	if (_numKeyFrames == 0 || pKeyframes == nullptr)
	{
		_keyFrames.clear();
		_numKeyFrames = 0;
		return E_FAIL;
	}

	_keyFrames.assign(pKeyframes, pKeyframes + _numKeyFrames);

	return S_OK;
}

void Channel::Get_ChannelTransform(float trackPosition, uint32& keyFrameIndex, float duration, bool isLoop, RootMotionFrame& outTransform)
{
	UNREFERENCED_PARAMETER(duration);
	UNREFERENCED_PARAMETER(isLoop);

	// 1. 키프레임이 없는 경우의 기본값 반환
	if (_keyFrames.empty())
	{
		outTransform.scale = Vec3::One;
		outTransform.rotation = Quat::Identity;
		outTransform.position = Vec3::Zero;
		return;
	}

	// 2. 키프레임이 1개이거나, 재생 시간이 첫 프레임보다 이전인 경우 (첫 프레임 고정)
	if (_keyFrames.size() == 1 || trackPosition <= _keyFrames.front().trackPosition)
	{
		const KEYFRAME& key = _keyFrames.front();
		outTransform.scale = key.scale;
		outTransform.rotation = key.rotation;
		outTransform.position = key.translation;
		keyFrameIndex = 0;
		return;
	}

	// 3. 재생 시간이 마지막 프레임보다 이후인 경우 (마지막 프레임 고정)
	if (trackPosition >= _keyFrames.back().trackPosition)
	{
		const KEYFRAME& key = _keyFrames.back();
		outTransform.scale = key.scale;
		outTransform.rotation = key.rotation;
		outTransform.position = key.translation;
		keyFrameIndex = static_cast<uint32>(_keyFrames.size() - 1);
		return;
	}

	// 4. 인덱스 유효성 검사 (아웃오브바운드 방지 및 애니메이션 루프/초기화 시 0으로 되돌리기)
	if (keyFrameIndex >= _numKeyFrames - 1 || trackPosition < _keyFrames[keyFrameIndex].trackPosition)
	{
		keyFrameIndex = 0;
	}

	// 5. 현재 trackPosition이 속한 구간(Index) 찾기 (이전 인덱스부터 순차 탐색)
	while (keyFrameIndex + 1 < _numKeyFrames && trackPosition >= _keyFrames[keyFrameIndex + 1].trackPosition)
	{
		++keyFrameIndex;
	}

	// 6. 보간(Interpolation) 수행
	const KEYFRAME& currentKey = _keyFrames[keyFrameIndex];
	const KEYFRAME& nextKey = _keyFrames[keyFrameIndex + 1];

	float ratio = (trackPosition - currentKey.trackPosition) / (nextKey.trackPosition - currentKey.trackPosition);

	outTransform.scale = Vec3::Lerp(currentKey.scale, nextKey.scale, ratio);
	outTransform.rotation = Quat::Slerp(currentKey.rotation, nextKey.rotation, ratio);
	outTransform.position = Vec3::Lerp(currentKey.translation, nextKey.translation, ratio);
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

Vec3 Channel::Get_TotalTranslation() const
{
    if (_keyFrames.size() < 2) return Vec3::Zero;

    return _keyFrames.back().translation - _keyFrames.front().translation;
}

Vec3 Channel::Get_FirstFramePosition() const
{
    return _keyFrames.empty() ? Vec3::Zero : _keyFrames.front().translation;
}

size_t Channel::Get_DebugEstimatedBytes() const
{
    return sizeof(Channel)
        + sizeof(KEYFRAME) * _keyFrames.capacity();
}

void Channel::Update_TransformationMatrix(uint32& currentKeyFrameIndex, float currentTrackPosition, float duration, const vector<Shared<Bone>>& bones, int32 rootNodeIndex, bool isLoop)
{
	if (_boneIndex == -1 || _boneIndex >= bones.size())
		return;

	RootMotionFrame transform{};
	Get_ChannelTransform(currentTrackPosition, currentKeyFrameIndex, duration, isLoop, transform);

    if (_boneIndex == rootNodeIndex)
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
