#pragma once

#include "UI_AnimationAssetSerializer.h"
#include "UI_Editor_Window.h"

namespace UIEditor
{
    class UI_Editor_Manager;

    class UI_Animation_View final : public UI_Editor_Window
    {
    public:
        explicit UI_Animation_View(UI_Editor_Manager& editorManager);
        ~UI_Animation_View() override = default;

    public:
        void Render() override;

    private:
        void Render_Toolbar();
        void Render_TargetSection();
        void Render_PlaybackSection();
        void Render_TimelineSection();
        void Render_TrackPanel();
        void Render_TimelineCanvas(const ImVec2& size);
        void Render_KeyPanel();

        void New_Asset();
        bool Save_Asset(bool forceSaveAs);
        bool Load_Asset();
        bool Confirm_DiscardDirtyAsset(const wchar_t* actionName);
        bool Show_SaveAssetDialog(wstring& outFilePath) const;
        bool Show_LoadAssetDialog(wstring& outFilePath) const;
        wstring Resolve_DefaultAnimationDirectory() const;
        void Ensure_AnimationExtension(wstring& filePath) const;

        void Bind_SelectedUI();
        void Clear_Target();
        void Resolve_TargetObject();
        void Sync_TargetNameBuffer();
        void Set_CurrentFrame(int32 frame);
        void Refresh_StaticPreview();
        void Clamp_Asset();
        void Sort_TrackKeys(UIAnimationTrack& track);

        UIAnimationTrack* Get_SelectedTrack();
        const UIAnimationTrack* Get_SelectedTrack() const;
        UIAnimationKeyframe* Get_SelectedKey();
        const UIAnimationKeyframe* Get_SelectedKey() const;
        float Get_TargetCurrentValue(UIAnimationTrackType trackType) const;
        void Add_Track(UIAnimationTrackType trackType);
        void Delete_SelectedTrack();
        void Add_KeyToSelectedTrack();
        void Delete_SelectedKey();
        void Select_Key(int32 trackIndex, int32 keyIndex);
        void Mark_AssetDirty();

    public:
        static Shared<UI_Animation_View> Create(UI_Editor_Manager& editorManager);

    private:
        UI_Editor_Manager& _editorManager;
        UIAnimationAsset _asset = {};
        wstring _assetFilePath = {};
        bool _assetDirty = false;
        bool _previewEnabled = true;
        int32 _currentFrame = 0;
        int32 _selectedTrackIndex = -1;
        int32 _selectedKeyIndex = -1;
        UIAnimationTrackType _pendingTrackType = UIAnimationTrackType::PositionX;
        char _targetNameBuffer[128] = {};
    };
}
