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
	for (auto& ch : prototype._channels)
		_channels.push_back(ch->Clone());
}

HRESULT Animation::Initialize(const ModelAnimationData& animData, const vector<ModelChannelData>& channels)
{
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
		_channels[i]->Update_TransformationMatrix(
			_currentKeyFrameIndices[i], _currentTrackPosition, _duration, bones, rootNodeIndex, isLoop);
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

		_channels[i]->Get_ChannelTransform(
			_currentTrackPosition, _currentKeyFrameIndices[i], _duration, isLoop, curTrans);
		_channels[i]->Update_Velocity(curTrans, _currentTrackPosition);

		nextAnim->_channels[i]->Get_ChannelTransform(
			nextAnim->_currentTrackPosition, nextAnim->_currentKeyFrameIndices[i], nextAnim->_duration, isLoop, nextTrans);
		nextAnim->_channels[i]->Update_Velocity(nextTrans, nextAnim->_currentTrackPosition);

		XMVECTOR tS = XMVectorLerp(XMLoadFloat3(&curTrans.scale), XMLoadFloat3(&nextTrans.scale), blendRatio);
		XMVECTOR tR = XMQuaternionSlerp(XMLoadFloat4(&curTrans.rotation), XMLoadFloat4(&nextTrans.rotation), blendRatio);
		XMVECTOR tP = XMVectorLerp(XMLoadFloat3(&curTrans.position), XMLoadFloat3(&nextTrans.position), blendRatio);

		XMMATRIX targetMatrix;

		if (_channels[i]->Get_BoneIndex() == rootNodeIndex)
		{
			targetMatrix = XMMatrixAffineTransformation(tS, XMVectorZero(), tR, XMVectorZero());
		}
		else
		{
			targetMatrix = XMMatrixAffineTransformation(tS, XMVectorZero(), tR, tP);
		}

		bones[_channels[i]->Get_BoneIndex()]->Update_TransformationMatrix(targetMatrix);
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
