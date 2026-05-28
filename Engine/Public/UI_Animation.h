#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

class UI_Base;

enum class UI_AnimationTrackType
{
    Fade,
    Move,
    Scale,
    Rotate,
    Color,
};

enum class UI_AnimationEasing
{
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
};

struct UI_AnimationTrack
{
    UI_AnimationTrackType type = UI_AnimationTrackType::Fade;
    bool enabled = true;
    Vec2 fromVec2{ 0.f, 0.f };
    Vec2 toVec2{ 0.f, 0.f };
    float fromFloat = 0.f;
    float toFloat = 0.f;
    Vec4 fromColor{ 1.f, 1.f, 1.f, 1.f };
    Vec4 toColor{ 1.f, 1.f, 1.f, 1.f };
};

struct UI_AnimationClip
{
    wstring name = L"Default";
    bool enabled = true;
    bool playOnOpen = true;
    float delay = 0.f;
    float duration = 0.35f;
    UI_AnimationEasing easing = UI_AnimationEasing::EaseOut;
    bool loop = false;
    vector<UI_AnimationTrack> tracks = {};
};

enum class UI_KeyframeAnimationTrackType
{
    PositionX,
    PositionY,
    ScaleX,
    ScaleY,
    Rotation,
    Color,
    Alpha,
};

enum class UI_KeyframeAnimationValueMode
{
    Absolute,
    RelativeTransform,
    TransitionFromCurrent,
    TransitionFromCurrentRelativeTransform,
};

enum class UI_StateAnimationEvent
{
    HoverEnter,
    HoverExit,
    PressedEnter,
    PressedExit,
    FocusedEnter,
    FocusedExit,
    DisabledEnter,
    DisabledExit,
    Count,
};

struct UI_KeyframeAnimationKey
{
    int32 frame = 0;
    float value = 0.f;
    Vec4 color{ 255.f, 255.f, 255.f, 255.f };
    UI_AnimationEasing easing = UI_AnimationEasing::Linear;
};

struct UI_KeyframeAnimationTrack
{
    UI_KeyframeAnimationTrackType type = UI_KeyframeAnimationTrackType::PositionX;
    bool enabled = true;
    UI_AnimationEasing easing = UI_AnimationEasing::Linear;
    vector<UI_KeyframeAnimationKey> keys = {};
};

struct UI_KeyframeAnimationAsset
{
    wstring name = L"NewUIAnimation";
    bool enabled = true;
    bool playOnOpen = true;
    UI_KeyframeAnimationValueMode valueMode = UI_KeyframeAnimationValueMode::Absolute;
    int32 fps = 60;
    int32 startFrame = 0;
    int32 endFrame = 60;
    bool loop = false;
    vector<UI_KeyframeAnimationTrack> tracks = {};
};

struct UI_StateAnimationTargetAction
{
    Weak<UI_Base> target = {};
    wstring animationName = {};
};

struct UI_StateAnimationProfile
{
    array<wstring, static_cast<size_t>(UI_StateAnimationEvent::Count)> eventAnimationNames = {};
    array<vector<UI_StateAnimationTargetAction>, static_cast<size_t>(UI_StateAnimationEvent::Count)> eventTargetActions = {};
};

struct UI_AnimationSample
{
    bool hasPosition = false;
    bool hasScale = false;
    bool hasRotation = false;
    bool hasColor = false;
    bool hasColorTrack = false;
    bool hasFadeTrack = false;
    Vec2 position{ 0.f, 0.f };
    Vec2 scale{ 1.f, 1.f };
    float rotationRadians = 0.f;
    Vec4 color{ 1.f, 1.f, 1.f, 1.f };
};

class ENGINE_DLL UI_AnimationPlayer final
{
public:
    UI_AnimationPlayer() = default;
    ~UI_AnimationPlayer() = default;

public:
    void Set_Clips(const vector<UI_AnimationClip>& clips);
    void Set_KeyframeAssets(const vector<UI_KeyframeAnimationAsset>& assets);
    void Add_KeyframeAsset(const UI_KeyframeAnimationAsset& asset);
    void Set_BaseTransformState(const Vec2& position, const Vec2& scale, float rotationRadians);
    void Set_TransitionStartState(const Vec2& position, const Vec2& scale, float rotationRadians);
    void Play_OnOpen();
    void Stop();
    bool Play_KeyframeAnimation(const wstring& animationName, bool restart = true); // 이름으로 keyframe animation 하나를 선택 재생
    bool Update(float deltaTime, UI_AnimationSample& outSample);

private:
    size_t Find_PlayOnOpenClipIndex() const;
    size_t Find_PlayOnOpenKeyframeAssetIndex() const;
    size_t Find_KeyframeAssetIndex(const wstring& animationName) const; // 이름으로 asset index 찾기

private:
    vector<UI_AnimationClip> _clips = {};
    vector<UI_KeyframeAnimationAsset> _keyframeAssets = {};
    size_t _activeClipIndex = numeric_limits<size_t>::max();
    size_t _activeKeyframeAssetIndex = numeric_limits<size_t>::max();
    bool _isPlaying = false;
    float _time = 0.f;
    Vec2 _basePosition{ 0.f, 0.f };
    Vec2 _baseScale{ 1.f, 1.f };
    float _baseRotationRadians = 0.f;
    Vec2 _transitionStartPosition{ 0.f, 0.f };
    Vec2 _transitionStartScale{ 1.f, 1.f };
    float _transitionStartRotationRadians = 0.f;
};

inline bool Try_ParseUIAnimationTrackType(const string& typeName, UI_AnimationTrackType& outType)
{
    if (typeName == "Fade")
    {
        outType = UI_AnimationTrackType::Fade;
        return true;
    }

    if (typeName == "Move")
    {
        outType = UI_AnimationTrackType::Move;
        return true;
    }

    if (typeName == "Scale")
    {
        outType = UI_AnimationTrackType::Scale;
        return true;
    }

    if (typeName == "Rotate")
    {
        outType = UI_AnimationTrackType::Rotate;
        return true;
    }

    if (typeName == "Color")
    {
        outType = UI_AnimationTrackType::Color;
        return true;
    }

    return false;
}

inline bool Try_ParseUIKeyframeAnimationTrackType(const string& typeName, UI_KeyframeAnimationTrackType& outType)
{
    if (typeName == "PositionX")
    {
        outType = UI_KeyframeAnimationTrackType::PositionX;
        return true;
    }

    if (typeName == "PositionY")
    {
        outType = UI_KeyframeAnimationTrackType::PositionY;
        return true;
    }

    if (typeName == "ScaleX")
    {
        outType = UI_KeyframeAnimationTrackType::ScaleX;
        return true;
    }

    if (typeName == "ScaleY")
    {
        outType = UI_KeyframeAnimationTrackType::ScaleY;
        return true;
    }

    if (typeName == "Rotation")
    {
        outType = UI_KeyframeAnimationTrackType::Rotation;
        return true;
    }

    if (typeName == "Color")
    {
        outType = UI_KeyframeAnimationTrackType::Color;
        return true;
    }

    if (typeName == "Alpha")
    {
        outType = UI_KeyframeAnimationTrackType::Alpha;
        return true;
    }

    return false;
}

inline bool Try_ParseUIAnimationEasing(const string& easingName, UI_AnimationEasing& outEasing)
{
    if (easingName == "Linear")
    {
        outEasing = UI_AnimationEasing::Linear;
        return true;
    }

    if (easingName == "EaseIn")
    {
        outEasing = UI_AnimationEasing::EaseIn;
        return true;
    }

    if (easingName == "EaseOut")
    {
        outEasing = UI_AnimationEasing::EaseOut;
        return true;
    }

    if (easingName == "EaseInOut")
    {
        outEasing = UI_AnimationEasing::EaseInOut;
        return true;
    }

    return false;
}

NS_END
