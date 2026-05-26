#include "pch.h"
#include "UI_AnimationAssetService.h"

namespace UIEditor
{
    namespace
    {
        ImVec4 Make_Color255WithAlpha01(const ImVec4& baseColor255, float alpha01)
        {
            ImVec4 color = Clamp_AnimationKeyframeColor(baseColor255);
            color.w = Clamp_AnimationKeyframeTrackValue(UI_AnimationKeyframeTrackType::Alpha, alpha01) * 255.f;
            return color;
        }
    }

    bool UI_AnimationAssetService::Apply_Preset(const UI_Document& document, UI_AnimationPresetType presetType, int32 durationFrames, float distance, float scaleAmount)
    {
        const UI_AnimationTargetBinding* target = Get_SelectedTarget();
        const UI_Object* targetObject = nullptr != target ? document.Find_Object(target->targetObjectId) : nullptr;
        if (nullptr == targetObject)
            return false;

        const int32 startFrame = _currentFrame;
        const int32 safeDuration = max(1, durationFrames);
        const int32 endFrame = startFrame + safeDuration;
        const int32 midFrame = startFrame + max(1, static_cast<int32>(roundf(static_cast<float>(safeDuration) * 0.6f)));
        const float safeDistance = max(1.f, distance);
        const float safeScaleAmount = max(0.01f, scaleAmount);
        const float baseX = targetObject->position.x;
        const float baseY = targetObject->position.y;
        const float baseScaleX = max(0.01f, targetObject->scale.x);
        const float baseScaleY = max(0.01f, targetObject->scale.y);
        const float baseRotation = targetObject->rotationRadians;
        const float baseAlpha = min(max(targetObject->color.w, 0.f), 1.f);
        const ImVec4 baseColor = Convert_AnimationColor01To255(targetObject->color);
        const ImVec4 transparentColor = Make_Color255WithAlpha01(baseColor, 0.f);

        _asset.endFrame = max(_asset.endFrame, endFrame);

        bool changed = false;
        auto key = [&](UI_AnimationKeyframeTrackType trackType, int32 frame, float value, UI_AnimationEasing easing)
        {
            changed |= Upsert_KeyValue(trackType, frame, value, easing, false);
        };
        auto colorKey = [&](int32 frame, const ImVec4& color, UI_AnimationEasing easing)
        {
            changed |= Upsert_KeyColor(frame, color, easing, false);
        };

        switch (presetType)
        {
        case UI_AnimationPresetType::FadeIn:
            colorKey(startFrame, transparentColor, UI_AnimationEasing::EaseOut);
            colorKey(endFrame, Make_Color255WithAlpha01(baseColor, baseAlpha), UI_AnimationEasing::EaseOut);
            break;
        case UI_AnimationPresetType::FadeOut:
            colorKey(startFrame, Make_Color255WithAlpha01(baseColor, baseAlpha), UI_AnimationEasing::EaseIn);
            colorKey(endFrame, transparentColor, UI_AnimationEasing::EaseIn);
            break;
        case UI_AnimationPresetType::SlideInUp:
            key(UI_AnimationKeyframeTrackType::PositionY, startFrame, baseY + safeDistance, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::PositionY, endFrame, baseY, UI_AnimationEasing::EaseOut);
            colorKey(startFrame, transparentColor, UI_AnimationEasing::EaseOut);
            colorKey(endFrame, Make_Color255WithAlpha01(baseColor, baseAlpha), UI_AnimationEasing::EaseOut);
            break;
        case UI_AnimationPresetType::SlideInDown:
            key(UI_AnimationKeyframeTrackType::PositionY, startFrame, baseY - safeDistance, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::PositionY, endFrame, baseY, UI_AnimationEasing::EaseOut);
            colorKey(startFrame, transparentColor, UI_AnimationEasing::EaseOut);
            colorKey(endFrame, Make_Color255WithAlpha01(baseColor, baseAlpha), UI_AnimationEasing::EaseOut);
            break;
        case UI_AnimationPresetType::SlideInLeft:
            key(UI_AnimationKeyframeTrackType::PositionX, startFrame, baseX - safeDistance, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::PositionX, endFrame, baseX, UI_AnimationEasing::EaseOut);
            colorKey(startFrame, transparentColor, UI_AnimationEasing::EaseOut);
            colorKey(endFrame, Make_Color255WithAlpha01(baseColor, baseAlpha), UI_AnimationEasing::EaseOut);
            break;
        case UI_AnimationPresetType::SlideInRight:
            key(UI_AnimationKeyframeTrackType::PositionX, startFrame, baseX + safeDistance, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::PositionX, endFrame, baseX, UI_AnimationEasing::EaseOut);
            colorKey(startFrame, transparentColor, UI_AnimationEasing::EaseOut);
            colorKey(endFrame, Make_Color255WithAlpha01(baseColor, baseAlpha), UI_AnimationEasing::EaseOut);
            break;
        case UI_AnimationPresetType::PopIn:
            key(UI_AnimationKeyframeTrackType::ScaleX, startFrame, baseScaleX * 0.86f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, startFrame, baseScaleY * 0.86f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleX, midFrame, baseScaleX * safeScaleAmount, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, midFrame, baseScaleY * safeScaleAmount, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleX, endFrame, baseScaleX, UI_AnimationEasing::EaseInOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, endFrame, baseScaleY, UI_AnimationEasing::EaseInOut);
            colorKey(startFrame, transparentColor, UI_AnimationEasing::EaseOut);
            colorKey(endFrame, Make_Color255WithAlpha01(baseColor, baseAlpha), UI_AnimationEasing::EaseOut);
            break;
        case UI_AnimationPresetType::TooltipAppear:
            key(UI_AnimationKeyframeTrackType::PositionY, startFrame, baseY + safeDistance * 0.35f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::PositionY, endFrame, baseY, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleX, startFrame, baseScaleX * 0.96f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, startFrame, baseScaleY * 0.96f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleX, endFrame, baseScaleX, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, endFrame, baseScaleY, UI_AnimationEasing::EaseOut);
            colorKey(startFrame, transparentColor, UI_AnimationEasing::EaseOut);
            colorKey(endFrame, Make_Color255WithAlpha01(baseColor, baseAlpha), UI_AnimationEasing::EaseOut);
            break;
        case UI_AnimationPresetType::ButtonHoverPulse:
            key(UI_AnimationKeyframeTrackType::ScaleX, startFrame, baseScaleX, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, startFrame, baseScaleY, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleX, midFrame, baseScaleX * 1.04f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, midFrame, baseScaleY * 1.04f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleX, endFrame, baseScaleX, UI_AnimationEasing::EaseInOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, endFrame, baseScaleY, UI_AnimationEasing::EaseInOut);
            break;
        case UI_AnimationPresetType::ButtonPressPop:
            key(UI_AnimationKeyframeTrackType::ScaleX, startFrame, baseScaleX, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, startFrame, baseScaleY, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleX, midFrame, baseScaleX * 0.94f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, midFrame, baseScaleY * 0.94f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleX, endFrame, baseScaleX, UI_AnimationEasing::EaseInOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, endFrame, baseScaleY, UI_AnimationEasing::EaseInOut);
            break;
        case UI_AnimationPresetType::SkillNodeReveal:
            key(UI_AnimationKeyframeTrackType::ScaleX, startFrame, baseScaleX * 0.75f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, startFrame, baseScaleY * 0.75f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleX, midFrame, baseScaleX * safeScaleAmount, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, midFrame, baseScaleY * safeScaleAmount, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::ScaleX, endFrame, baseScaleX, UI_AnimationEasing::EaseInOut);
            key(UI_AnimationKeyframeTrackType::ScaleY, endFrame, baseScaleY, UI_AnimationEasing::EaseInOut);
            key(UI_AnimationKeyframeTrackType::Rotation, startFrame, baseRotation - 0.08f, UI_AnimationEasing::EaseOut);
            key(UI_AnimationKeyframeTrackType::Rotation, endFrame, baseRotation, UI_AnimationEasing::EaseOut);
            colorKey(startFrame, transparentColor, UI_AnimationEasing::EaseOut);
            colorKey(endFrame, Make_Color255WithAlpha01(baseColor, baseAlpha), UI_AnimationEasing::EaseOut);
            break;
        default:
            return false;
        }

        if (changed)
            Mark_AssetDirty();

        return changed;
    }
}
