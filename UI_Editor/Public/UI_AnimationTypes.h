#pragma once

#include "Engine_Define.h"
#include "UI_Editor_ImGuiInclude.h"

NS_BEGIN(UIEditor)

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
    ImVec2 fromVec2 = ImVec2(0.f, 0.f);
    ImVec2 toVec2 = ImVec2(0.f, 0.f);
    float fromFloat = 0.f;
    float toFloat = 0.f;
    ImVec4 fromColor = ImVec4(1.f, 1.f, 1.f, 1.f);
    ImVec4 toColor = ImVec4(1.f, 1.f, 1.f, 1.f);
};

struct UI_AnimationClip
{
    string name = "Default";
    bool enabled = true;
    bool playOnOpen = true;
    float delay = 0.f;
    float duration = 0.35f;
    UI_AnimationEasing easing = UI_AnimationEasing::EaseOut;
    bool loop = false;
    vector<UI_AnimationTrack> tracks = {};
};

enum class UI_AnimationKeyframeTrackType
{
    PositionX,
    PositionY,
    ScaleX,
    ScaleY,
    Rotation,
    Alpha,
};

struct UI_AnimationKeyframe
{
    int32 frame = 0;
    float value = 0.f;
    UI_AnimationEasing easing = UI_AnimationEasing::Linear;
};

struct UI_AnimationKeyframeTrack
{
    UI_AnimationKeyframeTrackType type = UI_AnimationKeyframeTrackType::PositionX;
    bool enabled = true;
    UI_AnimationEasing easing = UI_AnimationEasing::Linear;
    vector<UI_AnimationKeyframe> keys = {};
};

struct UI_AnimationAsset
{
    string name = "NewUIAnimation";
    string targetName = "";
    uint32 targetObjectId = 0;
    int32 fps = 60;
    int32 startFrame = 0;
    int32 endFrame = 60;
    bool loop = false;
    vector<UI_AnimationKeyframeTrack> tracks = {};
};

inline const char* Get_AnimationTrackTypeName(UI_AnimationTrackType type)
{
    switch (type)
    {
    case UI_AnimationTrackType::Fade:
        return "Fade";
    case UI_AnimationTrackType::Move:
        return "Move";
    case UI_AnimationTrackType::Scale:
        return "Scale";
    case UI_AnimationTrackType::Rotate:
        return "Rotate";
    case UI_AnimationTrackType::Color:
        return "Color";
    default:
        return "Fade";
    }
}

inline bool Try_ParseAnimationTrackType(const string& typeName, UI_AnimationTrackType& outType)
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

inline const char* Get_AnimationKeyframeTrackTypeName(UI_AnimationKeyframeTrackType type)
{
    switch (type)
    {
    case UI_AnimationKeyframeTrackType::PositionX:
        return "PositionX";
    case UI_AnimationKeyframeTrackType::PositionY:
        return "PositionY";
    case UI_AnimationKeyframeTrackType::ScaleX:
        return "ScaleX";
    case UI_AnimationKeyframeTrackType::ScaleY:
        return "ScaleY";
    case UI_AnimationKeyframeTrackType::Rotation:
        return "Rotation";
    case UI_AnimationKeyframeTrackType::Alpha:
        return "Alpha";
    default:
        return "PositionX";
    }
}

inline bool Try_ParseAnimationKeyframeTrackType(const string& typeName, UI_AnimationKeyframeTrackType& outType)
{
    if (typeName == "PositionX")
    {
        outType = UI_AnimationKeyframeTrackType::PositionX;
        return true;
    }

    if (typeName == "PositionY")
    {
        outType = UI_AnimationKeyframeTrackType::PositionY;
        return true;
    }

    if (typeName == "ScaleX")
    {
        outType = UI_AnimationKeyframeTrackType::ScaleX;
        return true;
    }

    if (typeName == "ScaleY")
    {
        outType = UI_AnimationKeyframeTrackType::ScaleY;
        return true;
    }

    if (typeName == "Rotation")
    {
        outType = UI_AnimationKeyframeTrackType::Rotation;
        return true;
    }

    if (typeName == "Alpha")
    {
        outType = UI_AnimationKeyframeTrackType::Alpha;
        return true;
    }

    return false;
}

inline float Clamp_AnimationKeyframeTrackValue(UI_AnimationKeyframeTrackType type, float value)
{
    switch (type)
    {
    case UI_AnimationKeyframeTrackType::ScaleX:
    case UI_AnimationKeyframeTrackType::ScaleY:
        return max(0.01f, value);
    case UI_AnimationKeyframeTrackType::Alpha:
        return min(max(value, 0.f), 1.f);
    default:
        return value;
    }
}

inline const char* Get_AnimationEasingName(UI_AnimationEasing easing)
{
    switch (easing)
    {
    case UI_AnimationEasing::Linear:
        return "Linear";
    case UI_AnimationEasing::EaseIn:
        return "EaseIn";
    case UI_AnimationEasing::EaseOut:
        return "EaseOut";
    case UI_AnimationEasing::EaseInOut:
        return "EaseInOut";
    default:
        return "EaseOut";
    }
}

inline bool Try_ParseAnimationEasing(const string& easingName, UI_AnimationEasing& outEasing)
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

inline float Evaluate_AnimationEasing(UI_AnimationEasing easing, float t)
{
    t = min(max(t, 0.f), 1.f);

    switch (easing)
    {
    case UI_AnimationEasing::Linear:
        return t;
    case UI_AnimationEasing::EaseIn:
        return t * t;
    case UI_AnimationEasing::EaseOut:
        return 1.f - (1.f - t) * (1.f - t);
    case UI_AnimationEasing::EaseInOut:
    {
        const float inverse = -2.f * t + 2.f;
        return t < 0.5f ? 2.f * t * t : 1.f - inverse * inverse * 0.5f;
    }
    default:
        return t;
    }
}

NS_END
