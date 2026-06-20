#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

class GameObject;
class ModelCom;
class AnimNotify;
class AnimNotifyState;

struct AnimNotifyContext
{
    GameObject* owner = nullptr;
    ModelCom*   model = nullptr;

    string      modelGuid{};
    string      animationName{};

    float       previousTimeSec = 0.f;
    float       currentTimeSec = 0.f;
    float       deltaTime = 0.f;

    float       notifyStateProgress = 0.f;      // ANS 진행도
    float       notifyStateDurationSec = 0.f;   // ANS 전체 길이(초)

    bool        isLooping = false;  // 반복할지
    bool        wrapped = false;    // 
    bool        isPreview = false;  // 프리뷰 (애니메이션 뷰) 인지
};

struct AnimNotifyTrackDesc
{
    // 일단 테스트로 트랙 1개만
    string name{};
};

// AninNotify -> 단발성
struct AnimNotifyEventEntry
{
    int32               id = 0;
    float               timeSec = 0.f;
    int32               trackIndex = 0;

    Shared<AnimNotify>  notify{};
};

// AnimNotifyState -> start, update, end
struct AnimNotifyStateEntry
{
    int32                   id = 0;
    float                   startSec = 0.f;
    float                   durationSec = 0.f;
    int32                   trackIndex = 0;

    Shared<AnimNotifyState> notifyState;
};

// 애니메이션 하나에 대응하는 notify 묶음이다.
struct AnimNotifyData
{
    string  animationName{};
    int32   displayFps = 60;
    float   playSectionStartSec = 0.f;
    float   playSectionEndSec = -1.f;

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
    int32   version = 2;
    string  modelGuid{};
    vector<AnimNotifyData> animations;
};

// 실행되고있는 NotifyState
struct ActiveAnimNotifyState
{
    string                   animationName{};
    int32                    stateIndex = -1;
    float                    startSec = 0.f;
    float                    endSec = 0.f;
    float                    lastTimeSec = 0.f;
    Shared<AnimNotifyState>  notifyState{};
};

NS_END
