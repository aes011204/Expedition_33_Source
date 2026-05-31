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

enum class UI_AnimationAssetType
{
    Target_Animation,
    Public_Animation,
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
    Color,
    Alpha,
};

struct UI_AnimationKeyframe
{
    int32 frame = 0;
    float value = 0.f;
    ImVec4 color = ImVec4(255.f, 255.f, 255.f, 255.f);
    UI_AnimationEasing easing = UI_AnimationEasing::Linear;
};

struct UI_AnimationKeyframeTrack
{
    UI_AnimationKeyframeTrackType type = UI_AnimationKeyframeTrackType::PositionX;
    bool enabled = true;
    UI_AnimationEasing easing = UI_AnimationEasing::Linear;
    vector<UI_AnimationKeyframe> keys = {};
};

struct UI_AnimationTargetBinding
{
    string targetName = "";
    uint32 targetObjectId = 0;
    vector<UI_AnimationKeyframeTrack> tracks = {};
};

struct UI_AnimationAsset
{
    string name = "NewUIAnimation";
    UI_AnimationAssetType animationType = UI_AnimationAssetType::Public_Animation;
    int32 fps = 60;
    int32 startFrame = 0;
    int32 endFrame = 60;
    bool loop = false;
    vector<UI_AnimationKeyframeTrack> tracks = {};
    vector<UI_AnimationTargetBinding> targets = {};
};

struct UI_AnimationSequenceClip
{
    string displayName = "";
    wstring filePath = {};
    UI_AnimationAsset asset = {};
    int32 startFrame = 0;
    int32 durationFrames = 1;
    int32 resolvedTargetCount = 0;
};

struct UI_AnimationSequenceAsset
{
    int32 fps = 60;
    vector<UI_AnimationSequenceClip> clips = {};
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

struct UI_StateAnimationAsset
{
    string name = "NewUIStateAnimation";
    array<string, static_cast<size_t>(UI_StateAnimationEvent::Count)> eventAnimationPaths = {};
};

inline bool Is_TargetAnimation(const UI_AnimationAsset& asset)
{
    return asset.animationType == UI_AnimationAssetType::Target_Animation;
}

inline bool Is_PublicAnimation(const UI_AnimationAsset& asset)
{
    return asset.animationType == UI_AnimationAssetType::Public_Animation;
}

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
    case UI_AnimationKeyframeTrackType::Color:
        return "Color";
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

    if (typeName == "Color")
    {
        outType = UI_AnimationKeyframeTrackType::Color;
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

inline float Clamp_AnimationColorChannel255(float value)
{
    return min(max(value, 0.f), 255.f);
}

inline ImVec4 Clamp_AnimationKeyframeColor(ImVec4 color)
{
    color.x = Clamp_AnimationColorChannel255(color.x);
    color.y = Clamp_AnimationColorChannel255(color.y);
    color.z = Clamp_AnimationColorChannel255(color.z);
    color.w = Clamp_AnimationColorChannel255(color.w);
    return color;
}

inline ImVec4 Convert_AnimationColor01To255(const ImVec4& color)
{
    return Clamp_AnimationKeyframeColor(ImVec4(
        color.x * 255.f,
        color.y * 255.f,
        color.z * 255.f,
        color.w * 255.f));
}

inline ImVec4 Convert_AnimationColor255To01(const ImVec4& color)
{
    const ImVec4 clampedColor = Clamp_AnimationKeyframeColor(color);
    return ImVec4(
        clampedColor.x / 255.f,
        clampedColor.y / 255.f,
        clampedColor.z / 255.f,
        clampedColor.w / 255.f);
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

inline const char* Get_AnimationAssetTypeName(UI_AnimationAssetType type)
{
    switch (type)
    {
    case UI_AnimationAssetType::Target_Animation:
        return "Target_Animation";
    case UI_AnimationAssetType::Public_Animation:
        return "Public_Animation";
    default:
        return "Public_Animation";
    }
}

inline bool Try_ParseAnimationAssetType(const string& typeName, UI_AnimationAssetType& outType)
{
    if (typeName == "Target_Animation" || typeName == "PerTargetTracks")
    {
        outType = UI_AnimationAssetType::Target_Animation;
        return true;
    }

    if (typeName == "Public_Animation" || typeName == "SharedRelativeTracks")
    {
        outType = UI_AnimationAssetType::Public_Animation;
        return true;
    }

    return false;
}

inline const char* Get_StateAnimationEventName(UI_StateAnimationEvent eventType)
{
    switch (eventType)
    {
    case UI_StateAnimationEvent::HoverEnter:
        return "HoverEnter";
    case UI_StateAnimationEvent::HoverExit:
        return "HoverExit";
    case UI_StateAnimationEvent::PressedEnter:
        return "PressedEnter";
    case UI_StateAnimationEvent::PressedExit:
        return "PressedExit";
    case UI_StateAnimationEvent::FocusedEnter:
        return "FocusedEnter";
    case UI_StateAnimationEvent::FocusedExit:
        return "FocusedExit";
    case UI_StateAnimationEvent::DisabledEnter:
        return "DisabledEnter";
    case UI_StateAnimationEvent::DisabledExit:
        return "DisabledExit";
    default:
        return "HoverEnter";
    }
}

inline bool Try_ParseStateAnimationEvent(const string& eventName, UI_StateAnimationEvent& outEvent)
{
    for (int32 index = 0; index < static_cast<int32>(UI_StateAnimationEvent::Count); ++index)
    {
        const UI_StateAnimationEvent eventType = static_cast<UI_StateAnimationEvent>(index);
        if (eventName == Get_StateAnimationEventName(eventType))
        {
            outEvent = eventType;
            return true;
        }
    }

    return false;
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
