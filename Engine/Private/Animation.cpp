#include "pch.h"
#include "Animation.h"
#include "Channel.h"
#include "Bone.h"
#include <unordered_set>
#include "Debug_Profile.h"
#include <cmath>

NS_BEGIN(Engine)

Animation::Animation(const Animation& prototype)
	: _numChannels{ prototype._numChannels }
	, _currentKeyFrameIndices{ prototype._currentKeyFrameIndices }
	, _duration{ prototype._duration }
	, _tickPerSecond{ prototype._tickPerSecond }
	, _currentTrackPosition{ 0.f }
    , _channelIndexByBoneIndex{ prototype._channelIndexByBoneIndex }
    , _animationSpeed{ 1.f }
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

    // 초기화가 끝난 후 boneIndex -> channelIndex 캐싱해놔야 블렌딩 진입 시 매번 선형탐색하지 않는다.
    Build_ChannelIndexCache();

	return S_OK;
}

float Animation::Get_DurationSec() const
{
    return (_tickPerSecond > 0.f) ? (_duration / _tickPerSecond) : 0.f;
}

size_t Animation::Get_DebugEstimatedBytes() const
{
    size_t bytes = sizeof(Animation)
        + sizeof(Shared<Channel>) * _channels.capacity()
        + sizeof(uint32) * _currentKeyFrameIndices.capacity()
        + sizeof(int32) * _channelIndexByBoneIndex.capacity();

    for (const Shared<Channel>& channel : _channels)
    {
        if (channel)
            bytes += channel->Get_DebugEstimatedBytes();
    }

    return bytes;
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

void Animation::Set_StartTimeSec(float startSec)
{
    Set_CurrentTimeSec(startSec);
}

void Animation::Evaluate_CurrentPose(const vector<Shared<Bone>>& bones, int32 rootNodeIndex, bool isLoop)
{
    for (uint32 i = 0; i < _numChannels; ++i)
    {
        _channels[i]->Update_TransformationMatrix(_currentKeyFrameIndices[i], _currentTrackPosition, _duration, bones, rootNodeIndex, isLoop);
    }
}

float Animation::Get_Progress() const
{
    if (_duration <= 0.f)
        return 0.f;

    return clamp(_currentTrackPosition / _duration, 0.f, 1.f);
}

void Animation::Set_StartFrame(float startFrame)
{
    _currentTrackPosition = clamp(startFrame, 0.f, _duration);

    fill(_currentKeyFrameIndices.begin(), _currentKeyFrameIndices.end(), 0);

    for (auto& channel : _channels)
        channel->ResetVelocityState();
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

Vec3 Animation::Get_RootTranslation(int32 rootNodeIndex) const
{
    if (rootNodeIndex < 0) return Vec3::Zero;

    for (uint32 i = 0; i < _numChannels; ++i)
    {
        if (_channels[i]->Get_BoneIndex() == rootNodeIndex)
        {
            return _channels[i]->Get_TotalTranslation();
        }
    }
    return Vec3::Zero;
}

Vec3 Animation::Get_CurrentRootTranslation(int32 rootNodeIndex) const
{
    if (rootNodeIndex < 0) return Vec3::Zero;

    for (uint32 i = 0; i < _numChannels; ++i)
    {
        if (_channels[i]->Get_BoneIndex() == rootNodeIndex)
        {
            RootMotionFrame currentTransform{};
            uint32 keyIndex = _currentKeyFrameIndices[i];

            _channels[i]->Get_ChannelTransform(_currentTrackPosition, keyIndex, _duration, false, currentTransform);
            return currentTransform.position - _channels[i]->Get_FirstFramePosition();
        }
    }
    return Vec3::Zero;
}

bool Animation::Update_TransformationMatrices(float timeDelta, const vector<Shared<Bone>>& bones, bool isLoop, int32 rootNodeIndex)
{
    ENGINE_PROFILE_SCOPE_DETAIL("Animation::UpdatePose");

	_currentTrackPosition += _tickPerSecond * timeDelta * _animationSpeed;
	bool isFinished = false;

	if (_currentTrackPosition >= _duration)
	{
		if (!isLoop)
		{
			_currentTrackPosition = _duration;
			isFinished = true;
		}
		else
		{
			_currentTrackPosition = std::fmod(_currentTrackPosition, _duration);
		}
	}

	for (uint32 i = 0; i < _numChannels; ++i)
	{
		_channels[i]->Update_TransformationMatrix(_currentKeyFrameIndices[i], _currentTrackPosition, _duration, bones, rootNodeIndex, isLoop);
	}

	return isFinished;
}

void Animation::Blend_TransformMatrix(float currentTimeDelta, const Shared<Animation>& nextAnim, float blendRatio,
	const vector<Shared<Bone>>& bones, bool isLoop, int32 rootNodeIndex)
{
    ENGINE_PROFILE_SCOPE_DETAIL("Animation::BlendPose");

    // 어떤 뼈가 현재 애니메이션 루프에서 처리되었는지 기록 (해시셋 대신 vector<bool> 사용으로 메모리/속도 최적화)
    vector<bool> isBoneBlended(bones.size(), false);

    // 블렌딩 수행
    for (uint32 i = 0; i < _numChannels; ++i)
    {
        RootMotionFrame curTrans{}, nextTrans{};
        int32 boneIndex = _channels[i]->Get_BoneIndex();

        if (boneIndex < 0 || boneIndex >= bones.size())
            continue;

        isBoneBlended[boneIndex] = true;

        _channels[i]->Get_ChannelTransform(_currentTrackPosition, _currentKeyFrameIndices[i], _duration, isLoop, curTrans);

        int32 nextChannelIndex = nextAnim->Find_ChannelIndexByBoneIndex(boneIndex);

        if (nextChannelIndex != -1)
        {
            nextAnim->_channels[nextChannelIndex]->Get_ChannelTransform(
                nextAnim->_currentTrackPosition,
                nextAnim->_currentKeyFrameIndices[nextChannelIndex],
                nextAnim->_duration,
                isLoop,
                nextTrans);
        }
        else
        {
            // 다음 애니메이션에 없는 채널이라면, 이전 값 유지가 아닌 초기 포즈나 이전 값을 씀
            // (여기선 기존 방식대로 일단 curTrans 유지)
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

        if (boneIndex == rootNodeIndex)
        {
            _channels[i]->Update_Velocity(curTrans, _currentTrackPosition);

            if (nextChannelIndex != -1)
                nextAnim->_channels[nextChannelIndex]->Update_Velocity(nextTrans, nextAnim->_currentTrackPosition);
        }
    }

    // 2. 다음 애니메이션에는 있지만 현재 애니메이션에는 없는 뼈 처리 (스냅 방지)
    for (uint32 i = 0; i < nextAnim->_numChannels; ++i)
    {
        int32 boneIndex = nextAnim->_channels[i]->Get_BoneIndex();

        if (boneIndex < 0 || boneIndex >= bones.size())
            continue;

        if (!isBoneBlended[boneIndex])
        {
            RootMotionFrame curTrans{}, nextTrans{};

            // 현재 뼈의 이전 프레임 로컬 상태를 curTrans로 삼는다 (스냅 방지 및 부드러운 전환)
            Matrix curMatrix = bones[boneIndex]->Get_TransformationMatrix();
            curMatrix.Decompose(curTrans.scale, curTrans.rotation, curTrans.position);

            nextAnim->_channels[i]->Get_ChannelTransform(
                nextAnim->_currentTrackPosition,
                nextAnim->_currentKeyFrameIndices[i],
                nextAnim->_duration,
                isLoop,
                nextTrans);

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

            if (boneIndex == rootNodeIndex)
            {
                nextAnim->_channels[i]->Update_Velocity(nextTrans, nextAnim->_currentTrackPosition);
            }
        }
    }

	// 현재 애니메이션 시간 갱신
	_currentTrackPosition += _tickPerSecond * currentTimeDelta * _animationSpeed;
    if (_currentTrackPosition >= _duration)
        _currentTrackPosition = _duration;

	// 다음 애니메이션 시간 갱신
	nextAnim->_currentTrackPosition += nextAnim->_tickPerSecond * currentTimeDelta;
	if (nextAnim->_currentTrackPosition >= nextAnim->_duration)
		nextAnim->_currentTrackPosition = isLoop ? std::fmod(nextAnim->_currentTrackPosition, nextAnim->_duration) : nextAnim->_duration;
}

void Animation::Build_ChannelIndexCache()
{
    int32 maxBoneIndex = -1;

    for (const Shared<Channel>& channel : _channels)
    {
        if (channel == nullptr)
            continue;

        maxBoneIndex = max(maxBoneIndex, channel->Get_BoneIndex());
    }

    if (maxBoneIndex < 0)
    {
        _channelIndexByBoneIndex.clear();
        return;
    }

    _channelIndexByBoneIndex.assign(static_cast<size_t>(maxBoneIndex) + 1, -1);

    for (uint32 i = 0; i < _channels.size(); ++i)
    {
        if (_channels[i] == nullptr)
            continue;

        const int32 boneIndex = _channels[i]->Get_BoneIndex();
        if (boneIndex < 0)
            continue;

        const size_t sboneIndex = static_cast<size_t>(boneIndex);

        if (_channelIndexByBoneIndex[sboneIndex] == -1)
            _channelIndexByBoneIndex[sboneIndex] = static_cast<int32>(i);
    }

}

int32 Animation::Find_ChannelIndexByBoneIndex(int32 boneIndex) const
{
    if (boneIndex < 0)
        return -1;

    if (boneIndex >= static_cast<int32>(_channelIndexByBoneIndex.size()))
        return -1;

    return _channelIndexByBoneIndex[static_cast<size_t>(boneIndex)];
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
