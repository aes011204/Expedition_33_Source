#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Animation final : public Base
{
public:
    Animation();
    Animation(const Animation& prototype);
    ~Animation() override;

public:
    HRESULT Initialize(const aiAnimation* aiAnimation, class Model* model);
    bool Update_TransformationMatrices(float timeDelta, const vector<Shared<class Bone>>& bones, bool isLoop);

private:
    uint32 _numChannels{};
    vector<Shared<class Channel>> _channels;

    vector<uint32> _currentKeyFrameIndices;

    float _duration{};
    float _tickPerSecond{};
    float _currentTrackPosition{};

public:
    static Shared<Animation> Create(const aiAnimation* aiAnimation, class Model* model);
    Shared<Animation> Clone();
    void Free() override;
};

NS_END
