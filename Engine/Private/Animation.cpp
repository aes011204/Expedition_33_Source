#include "Animation.h"

#include "Channel.h"
#include "Bone.h"
#include <cmath>

NS_BEGIN(Engine)

Animation::Animation(const Animation& prototype)
	: _numChannels{ prototype._numChannels }
	, _currentKeyFrameIndices{ prototype._currentKeyFrameIndices }
	, _duration{ prototype._duration }
	, _tickPerSecond{ prototype._tickPerSecond }
	, _currentTrackPosition{ 0.f }
{
    _animationName = prototype._animationName;
	for (auto& ch : prototype._channels)
		_channels.push_back(ch->Clone());
}


HRESULT Animation::Initialize(const ModelAnimationData& animData, const vector<ModelChannelData>& channels)
{
    _animationName = animData.name;
	_duration = static_cast<float>(animData.duration);
	_tickPerSecond = static_cast<float>(animData.ticksPerSecond);
	_numChannels = animData.numChannels;
	_currentKeyFrameIndices.resize(_numChannels, 0);

	for (uint32 i = 0; i < _numChannels; ++i)
	{
		auto channel = Channel::Create(channels[i]);
		if (nullptr == channel)
			return E_FAIL;

		_channels.push_back(channel);
	}

	return S_OK;
}

float Animation::Get_DurationSec() const
{
    return (_tickPerSecond > 0.f) ? (_duration / _tickPerSecond) : 0.f;
}

float Animation::Get_CurrentTimeSec() const
{
    return (_tickPerSecond > 0.f) ? (_currentTrackPosition / _tickPerSecond) : 0.f;
}

void Animation::Set_CurrentTimeSec(float timeSec)
{
    const float durationSec = Get_DurationSec();
    const float clampedSec = ::clamp(timeSec, 0.f, durationSec);

    _currentTrackPosition = clampedSec * _tickPerSecond;
    ::fill(_currentKeyFrameIndices.begin(), _currentKeyFrameIndices.end(), 0);

    for (auto& ch : _channels)
        ch->ResetVelocityState();
}

void Animation::Evaluate_CurrentPose(const vector<Shared<Bone>>& bones, int32 rootNodeIndex, bool isLoop)
{
    for (uint32 i = 0; i < _numChannels; ++i)
    {
        _channels[i]->Update_TransformationMatrix(_currentKeyFrameIndices[i], _currentTrackPosition, _duration, bones, rootNodeIndex, isLoop);
    }
}

void Animation::Set_Progress(float progress)
{
	_currentTrackPosition = _duration * progress;
	std::fill(_currentKeyFrameIndices.begin(), _currentKeyFrameIndices.end(), 0);

	for (auto& ch : _channels)
		ch->ResetVelocityState();
}

void Animation::Reset()
{
	Set_Progress(0.f);
}

const RootMotionFrame& Animation::Get_TransformVelocity(int32 rootNodeIndex) const
{
	static RootMotionFrame emptyFrame{};

	for (const auto& ch : _channels)
	{
		if (ch->Get_BoneIndex() == rootNodeIndex)
			return ch->Get_TransformVelocity();
	}

	return emptyFrame;
}

bool Animation::Update_TransformationMatrices(float timeDelta, const vector<Shared<Bone>>& bones, bool isLoop, int32 rootNodeIndex)
{
	_currentTrackPosition += _tickPerSecond * timeDelta;

	if (_currentTrackPosition >= _duration)
	{
		if (!isLoop)
		{
			_currentTrackPosition = _duration;
			return true;
		}

		_currentTrackPosition = std::fmod(_currentTrackPosition, _duration);
	}

	for (uint32 i = 0; i < _numChannels; ++i)
	{
		_channels[i]->Update_TransformationMatrix(_currentKeyFrameIndices[i], _currentTrackPosition, _duration, bones, rootNodeIndex, isLoop);
	}

	return false;
}

void Animation::Blend_TransformMatrix(float timeDelta, const Shared<Animation>& nextAnim, float blendRatio,
	const vector<Shared<Bone>>& bones, bool isLoop, int32 rootNodeIndex)
{
	/* 현재 애니메이션 시간 갱신 */
	_currentTrackPosition += _tickPerSecond * timeDelta;
	if (_currentTrackPosition >= _duration)
		_currentTrackPosition = isLoop ? std::fmod(_currentTrackPosition, _duration) : _duration;

	/* 다음 애니메이션 시간 갱신 */
	nextAnim->_currentTrackPosition += nextAnim->_tickPerSecond * timeDelta;
	if (nextAnim->_currentTrackPosition >= nextAnim->_duration)
		nextAnim->_currentTrackPosition = isLoop ? std::fmod(nextAnim->_currentTrackPosition, nextAnim->_duration) : nextAnim->_duration;

    /* 블렌딩 수행 */
    for (uint32 i = 0; i < _numChannels; ++i)
    {
        RootMotionFrame curTrans{}, nextTrans{};
        int32 boneIndex = _channels[i]->Get_BoneIndex();

        if (boneIndex < 0 || boneIndex >= bones.size())
            continue;

        _channels[i]->Get_ChannelTransform(_currentTrackPosition, _currentKeyFrameIndices[i], _duration, isLoop, curTrans);
        _channels[i]->Update_Velocity(curTrans, _currentTrackPosition);

        int32 nextChannelIndex = -1;
        for (uint32 j = 0; j < nextAnim->_numChannels; ++j)
        {
            if (nextAnim->_channels[j]->Get_BoneIndex() == boneIndex)
            {
                nextChannelIndex = static_cast<int32>(j);
                break;
            }
        }

        if (nextChannelIndex != -1)
        {
            nextAnim->_channels[nextChannelIndex]->Get_ChannelTransform(
                nextAnim->_currentTrackPosition, nextAnim->_currentKeyFrameIndices[nextChannelIndex], nextAnim->_duration, isLoop, nextTrans);
            nextAnim->_channels[nextChannelIndex]->Update_Velocity(nextTrans, nextAnim->_currentTrackPosition);
        }
        else
        {
            nextTrans = curTrans;
        }

        Vec3 transScale = Vec3::Lerp(curTrans.scale, nextTrans.scale, blendRatio);
        Quat transRotation = Quat::Slerp(curTrans.rotation, nextTrans.rotation, blendRatio);
        Vec3 transPosition = Vec3::Lerp(curTrans.position, nextTrans.position, blendRatio);

        Matrix targetMatrix{};
        if (boneIndex == rootNodeIndex) {
            targetMatrix = XMMatrixAffineTransformation(transScale, Vec3::Zero, transRotation, Vec3::Zero);
        }
        else {
            targetMatrix = XMMatrixAffineTransformation(transScale, Vec3::Zero, transRotation, transPosition);
        }

        bones[boneIndex]->Update_TransformationMatrix(targetMatrix);
    }
}

Shared<Animation> Animation::Create(const ModelAnimationData& animData, const vector<ModelChannelData>& channels)
{
	auto instance = make_shared<Animation>();

	if (FAILED(instance->Initialize(animData, channels)))
	{
		LOG_ERROR("[MODEL] Failed to Created : Animation");
		return nullptr;
	}
	return instance;
}

Shared<Animation> Animation::Clone()
{
	return make_shared<Animation>(*this);
}

void Animation::Free()
{
	__super::Free();

	_channels.clear();
}

NS_END
