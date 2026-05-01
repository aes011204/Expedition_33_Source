#include "Animation.h"

#include "Channel.h"

Animation::Animation()
{
}

Animation::Animation(const Animation& prototype)
    : _duration{ prototype._duration }
    , _tickPerSecond{ prototype._tickPerSecond }
    , _currentTrackPosition{ prototype._currentTrackPosition }
    , _numChannels{ prototype._numChannels }
    , _channels{ prototype._channels }
    , _currentKeyFrameIndices{ prototype._currentKeyFrameIndices }
{
    for (auto& channel : _channels)
        Safe_AddRef(channel);
}

Animation::~Animation()
{
    Free();
}

HRESULT Animation::Initialize(const aiAnimation* aiAnimation, class Model* model)
{
    _duration = static_cast<float>(aiAnimation->mDuration);
    _tickPerSecond = static_cast<float>(aiAnimation->mTicksPerSecond);

    _numChannels = aiAnimation->mNumChannels;

    _currentKeyFrameIndices.resize(_numChannels);

    for (size_t i = 0; i < _numChannels; i++)
    {
        Shared<Channel> channel = Channel::Create(aiAnimation->mChannels[i], model);
        if (nullptr == channel)
            return E_FAIL;

        _channels.push_back(channel);
    }

    return S_OK;
}

bool Animation::Update_TransformationMatrices(float timeDelta, const vector<Shared<class Bone>>& bones, bool isLoop)
{
    _currentTrackPosition += _tickPerSecond * timeDelta;

    if (_currentTrackPosition >= _duration)
    {
        if (false == isLoop)
            return true;

        _currentTrackPosition = 0.f;
    }

    // for (auto& channel : _channels)
    for (size_t i = 0; i < _numChannels; i++)
        _channels[i]->Update_TransformationMatrix(&_currentKeyFrameIndices[i], _currentTrackPosition, bones);

    return false;
}

Shared<Animation> Animation::Create(const aiAnimation* aiAnimation, class Model* model)
{
    auto instance = make_shared<Animation>();

    if (FAILED(instance->Initialize(aiAnimation, model)))
    {
        MSG_BOX("Failed to Created : Animation");
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

    for (auto& channel : _channels)
        Safe_Release(channel);

    _channels.clear();
}
