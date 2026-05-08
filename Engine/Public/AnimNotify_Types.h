#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

struct AnimNotifyTrackDesc
{
    // 일단 테스트로 트랙 1개만
    string name{};
};

// AninNotify -> 단발성
struct AnimNotifyEventEntry
{
    int32   id = 0;
    float   timeSec = 0.f;
    int32   trackIndex = 0;
    string  name{};
};

// AnimNotifyState -> start, update, end
struct AnimNotifyStateEntry
{
    int32   id = 0;
    float   startSec = 0.f;
    float   durationSec = 0.f;
    int32   trackIndex = 0;
    string  name{};
};

// 애니메이션 하나에 대응하는 notify 묶음이다.
struct AnimNotifyData
{
    string  animationName{};
    int32   displayFps = 60;

    // v1은 기본 트랙 하나씩만 유지한다.
    vector<AnimNotifyTrackDesc> notifyTracks;
    vector<AnimNotifyTrackDesc> notifyStateTracks;

    int32   nextNotifyId = 1;
    int32   nextStateId = 1;

    vector<AnimNotifyEventEntry> notifies;
    vector<AnimNotifyStateEntry> notifyStates;
};

// modelGuid 하나에 대응하는 저장 루트다.
struct AnimNotifyAsset
{
    int32   version = 1;
    string  modelGuid{};
    vector<AnimNotifyData> animations;
};

NS_END
