#pragma once

#include "UI_AnimationAssetSerializer.h"
#include "UI_Document.h"

namespace UIEditor
{
    class UI_AnimationSequenceService final
    {
    public:
        UI_AnimationSequenceService() = default;
        ~UI_AnimationSequenceService() = default;

    public:
        const UI_AnimationSequenceAsset& Get_Sequence() const { return _sequence; }
        UI_AnimationSequenceAsset& Get_MutableSequence() { return _sequence; }
        int32 Get_CurrentFrame() const { return _currentFrame; }
        int32 Get_SelectedClipIndex() const { return _selectedClipIndex; }
        int32 Get_TotalFrames() const;
        bool Is_Empty() const { return _sequence.clips.empty(); }
        bool Has_AnyResolvedTarget() const;

        bool Add_ClipsFromDialog(const UI_Document& document);
        void Clear();
        bool Remove_SelectedClip();
        bool Move_SelectedClip(int32 direction);
        void Select_Clip(int32 clipIndex);
        void Set_CurrentFrame(int32 frame);
        void Set_Fps(int32 fps);
        void Resolve_TargetObjects(const UI_Document& document);
        int32 Find_ClipIndexAtFrame(int32 frame) const;

    private:
        bool Show_LoadClipsDialog(vector<wstring>& outFilePaths) const;
        wstring Resolve_DefaultAnimationDirectory() const;
        bool Add_ClipFromFile(const wstring& filePath, const UI_Document& document);
        int32 Calculate_ClipDurationFrames(const UI_AnimationAsset& asset) const;
        void Rebuild_Timing();
        void Clamp_CurrentFrame();
        void Resolve_TargetObjectsForClip(UI_AnimationSequenceClip& clip, const UI_Document& document) const;

    private:
        UI_AnimationSequenceAsset _sequence = {};
        int32 _currentFrame = 0;
        int32 _selectedClipIndex = -1;
    };
}
