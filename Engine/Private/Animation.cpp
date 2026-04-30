#include "Animation.h"

#include "Channel.h"

Animation::Animation()
{
}

Animation::Animation(const Animation& Prototype)
	: _duration { Prototype._duration }
	, _tickPerSecond { Prototype._tickPerSecond }
	, _currentTrackPosition { Prototype._currentTrackPosition }
	, _numChannels { Prototype._numChannels }
	, _channels { Prototype._channels }
	, _currentKeyFrameIndices { Prototype._currentKeyFrameIndices}
{
	for (auto& pChannel : _channels)
		Safe_AddRef(pChannel);
}

HRESULT Animation::Initialize(const aiAnimation* pAIAnimation, class Model* pModel)
{
	_duration = pAIAnimation->mDuration;
	_tickPerSecond = pAIAnimation->mTicksPerSecond;

	_numChannels = pAIAnimation->mNumChannels;

	_currentKeyFrameIndices.resize(_numChannels);

	for (size_t i = 0; i < _numChannels; i++)
	{
		Shared<Channel> pChannel = Channel::Create(pAIAnimation->mChannels[i], pModel);
		if (nullptr == pChannel)
			return E_FAIL;

		_channels.push_back(pChannel);	
	}

	return S_OK;
}

bool Animation::Update_TransformationMatrices(float fTimeDelta, const vector<Shared<class Bone>>& Bones, bool isLoop)
{
	_currentTrackPosition += _tickPerSecond * fTimeDelta;

	if (_currentTrackPosition >= _duration)
	{
		if (false == isLoop)
			return true;

		_currentTrackPosition = 0.f;		
	}	

	

	// for (auto& pChannel : _channels)
	for (size_t i = 0; i < _numChannels; i++)	
	{
		_channels[i]->Update_TransformationMatrix(&_currentKeyFrameIndices[i], _currentTrackPosition, Bones);
	}

	return false;
}

Shared<Animation> Animation::Create(const aiAnimation* pAIAnimation, class Model* pModel)
{
	Shared<Animation> pInstance = MakeSharedFromRaw(new Animation());

	if (FAILED(pInstance->Initialize(pAIAnimation, pModel)))
	{
		MSG_BOX("Failed to Created : Animation");
		Safe_Release(pInstance);
	}
	return pInstance;
}

Shared<Animation> Animation::Clone()
{
	return MakeSharedFromRaw(new Animation(*this));
}


void Animation::Free()
{
	__super::Free();


	for (auto& pChannel : _channels)
		Safe_Release(pChannel);

	_channels.clear();
}
