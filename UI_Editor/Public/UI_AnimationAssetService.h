#pragma once

#include "UI_AnimationAssetSerializer.h"
#include "UI_Document.h"

namespace UIEditor
{
    enum class UI_AnimationPresetType
    {
        FadeIn,
        FadeOut,
        SlideInUp,
        SlideInDown,
        SlideInLeft,
        SlideInRight,
        PopIn,
        TooltipAppear,
        ButtonHoverPulse,
        ButtonPressPop,
        SkillNodeReveal,
    };

    class UI_AnimationAssetService final
    {
    public:
        UI_AnimationAssetService();
        ~UI_AnimationAssetService() = default;

    public:
        const UI_AnimationAsset& Get_Asset() const { return _asset; }
        UI_AnimationAsset& Get_MutableAsset() { return _asset; }
        const wstring& Get_AssetFilePath() const { return _assetFilePath; }
        bool Is_Dirty() const { return _assetDirty; }
        bool Is_PreviewEnabled() const { return _previewEnabled; }
        void Set_PreviewEnabled(bool previewEnabled) { _previewEnabled = previewEnabled; }
        int32 Get_CurrentFrame() const { return _currentFrame; }
        int32 Get_SelectedTrackIndex() const { return _selectedTrackIndex; }
        int32 Get_SelectedKeyIndex() const { return _selectedKeyIndex; }
        UI_AnimationKeyframeTrackType Get_PendingTrackType() const { return _pendingTrackType; }
        void Set_PendingTrackType(UI_AnimationKeyframeTrackType trackType) { _pendingTrackType = trackType; }

        void New_Asset();
        bool Save_Asset(bool forceSaveAs);
        bool Load_Asset();
        bool Confirm_DiscardDirtyAsset(const wchar_t* actionName);

        bool Bind_SelectedUI(const UI_Object* selectedObject);
        void Clear_Target();
        void Resolve_TargetObject(const UI_Document& document);
        void Set_AssetName(const string& assetName);
        void Set_TargetName(const string& targetName, const UI_Document& document);
        void Set_CurrentFrame(int32 frame);
        void Clamp_Asset();
        void Normalize_AllTracks();

        UI_AnimationKeyframeTrack* Get_SelectedTrack();
        const UI_AnimationKeyframeTrack* Get_SelectedTrack() const;
        UI_AnimationKeyframe* Get_SelectedKey();
        const UI_AnimationKeyframe* Get_SelectedKey() const;

        float Get_TargetCurrentValue(const UI_Document& document, UI_AnimationKeyframeTrackType trackType) const;
        bool Add_Track(UI_AnimationKeyframeTrackType trackType);
        bool Delete_SelectedTrack();
        bool Add_KeyToSelectedTrack(const UI_Document& document);
        bool Add_CurrentValueKeyToTrack(const UI_Document& document, UI_AnimationKeyframeTrackType trackType);
        bool Add_CurrentValueKeysToTransformTracks(const UI_Document& document);
        bool Apply_Preset(const UI_Document& document, UI_AnimationPresetType presetType, int32 durationFrames, float distance, float scaleAmount);
        bool Copy_SelectedKey();
        bool Paste_CopiedKey();
        bool Has_CopiedKey() const { return _hasCopiedKey; }
        bool Delete_SelectedKey();
        void Select_Track(int32 trackIndex);
        bool Select_Key(int32 trackIndex, int32 keyIndex);
        bool Set_SelectedKeyFrame(int32 frame, int32& outFrame);
        bool Set_SelectedKeyValue(float value);
        bool Set_SelectedTrackEasing(UI_AnimationEasing easing, bool applyToKeys);
        bool Set_SelectedKeyEasing(UI_AnimationEasing easing);
        void Mark_AssetDirty();

    private:
        bool Show_SaveAssetDialog(wstring& outFilePath) const;
        bool Show_LoadAssetDialog(wstring& outFilePath) const;
        wstring Resolve_DefaultAnimationDirectory() const;
        void Ensure_AnimationExtension(wstring& filePath) const;
        int32 Find_TrackIndex(UI_AnimationKeyframeTrackType trackType) const;
        int32 Find_KeyIndexAtFrame(const UI_AnimationKeyframeTrack& track, int32 frame) const;
        int32 Ensure_Track(UI_AnimationKeyframeTrackType trackType);
        bool Upsert_CurrentValueKey(const UI_Document& document, int32 trackIndex, bool selectKey);
        bool Upsert_KeyValue(UI_AnimationKeyframeTrackType trackType, int32 frame, float value, UI_AnimationEasing easing, bool selectKey);
        void Sort_TrackKeys(UI_AnimationKeyframeTrack& track);

    private:
        UI_AnimationAsset _asset = {};
        wstring _assetFilePath = {};
        bool _assetDirty = false;
        bool _previewEnabled = true;
        int32 _currentFrame = 0;
        int32 _selectedTrackIndex = -1;
        int32 _selectedKeyIndex = -1;
        UI_AnimationKeyframeTrackType _pendingTrackType = UI_AnimationKeyframeTrackType::PositionX;
        bool _hasCopiedKey = false;
        UI_AnimationKeyframeTrackType _copiedKeyTrackType = UI_AnimationKeyframeTrackType::PositionX;
        UI_AnimationKeyframe _copiedKey = {};
    };
}
