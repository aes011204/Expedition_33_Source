#pragma once

#include "UI_AnimationAssetService.h"
#include "UI_AnimationTimelineView.h"
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
        void Refresh_TargetBindings();

    private:
        void Render_Toolbar();
        void Render_TargetSection();
        void Render_PlaybackSection();
        void Render_TimelineSection();
        void Render_PresetPanel();
        void Render_TrackPanel();
        void Render_KeyPanel();

        void Sync_AssetNameBuffer();
        void Sync_TargetNameBuffer();
        void Set_CurrentFrame(int32 frame);
        void Mark_StaticPreviewDirty();
        void Request_StaticPreviewRefresh();
        void Refresh_StaticPreview();
        void Handle_TimelineClick(const UI_AnimationTimelineClickResult& clickResult);
        void Handle_Shortcuts();

    public:
        static Shared<UI_Animation_View> Create(UI_Editor_Manager& editorManager);

    private:
        UI_Editor_Manager& _editorManager;
        UI_AnimationAssetService _assetService = {};
        UI_AnimationTimelineView _timelineView = {};
        char _assetNameBuffer[128] = {};
        char _targetNameBuffer[128] = {};
        int32 _presetDurationFrames = 18;
        float _presetDistance = 48.f;
        float _presetScaleAmount = 1.08f;
        bool _staticPreviewDirty = true;
        int32 _lastStaticPreviewFrame = INT_MIN;
    };
}
