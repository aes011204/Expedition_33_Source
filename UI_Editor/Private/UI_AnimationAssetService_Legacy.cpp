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

    void UI_AnimationAssetService::Migrate_LegacyAlphaTracks(const UI_Document& document)
    {
        bool changed = false;
        for (UI_AnimationTargetBinding& target : _asset.targets)
        {
            const UI_Object* targetObject = document.Find_Object(target.targetObjectId);
            const ImVec4 baseColor = nullptr != targetObject
                ? Convert_AnimationColor01To255(targetObject->color)
                : ImVec4(255.f, 255.f, 255.f, 255.f);

            int32 colorTrackIndex = -1;
            for (int32 trackIndex = 0; trackIndex < static_cast<int32>(target.tracks.size()); ++trackIndex)
            {
                if (target.tracks[trackIndex].type == UI_AnimationKeyframeTrackType::Color)
                {
                    colorTrackIndex = trackIndex;
                    break;
                }
            }

            for (int32 trackIndex = 0; trackIndex < static_cast<int32>(target.tracks.size()); ++trackIndex)
            {
                UI_AnimationKeyframeTrack& track = target.tracks[trackIndex];
                if (track.type != UI_AnimationKeyframeTrackType::Alpha)
                    continue;

                if (colorTrackIndex >= 0 && colorTrackIndex != trackIndex)
                {
                    UI_AnimationKeyframeTrack& colorTrack = target.tracks[colorTrackIndex];
                    for (const UI_AnimationKeyframe& alphaKey : track.keys)
                    {
                        UI_AnimationKeyframe colorKey = alphaKey;
                        colorKey.color = Make_Color255WithAlpha01(baseColor, alphaKey.value);

                        const int32 existingKeyIndex = Find_KeyIndexAtFrame(colorTrack, colorKey.frame);
                        if (existingKeyIndex >= 0)
                        {
                            colorTrack.keys[existingKeyIndex].color.w = colorKey.color.w;
                            colorTrack.keys[existingKeyIndex].easing = colorKey.easing;
                        }
                        else
                        {
                            colorTrack.keys.push_back(colorKey);
                        }
                    }

                    Sort_TrackKeys(colorTrack);
                    track.keys.clear();
                }
                else
                {
                    track.type = UI_AnimationKeyframeTrackType::Color;
                    for (UI_AnimationKeyframe& key : track.keys)
                        key.color = Make_Color255WithAlpha01(baseColor, key.value);
                    Sort_TrackKeys(track);
                    colorTrackIndex = trackIndex;
                }

                changed = true;
            }

            target.tracks.erase(
                remove_if(
                    target.tracks.begin(),
                    target.tracks.end(),
                    [](const UI_AnimationKeyframeTrack& track)
                    {
                        return track.type == UI_AnimationKeyframeTrackType::Alpha && track.keys.empty();
                    }),
                target.tracks.end());
        }

        if (changed)
        {
            if (_pendingTrackType == UI_AnimationKeyframeTrackType::Alpha)
                _pendingTrackType = UI_AnimationKeyframeTrackType::Color;

            Clamp_SelectedIndices();
            Mark_AssetDirty();
        }
    }
}
