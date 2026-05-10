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

    private:
        void Render_Toolbar();
        void Render_TargetSection();
        void Render_PlaybackSection();
        void Render_TimelineSection();
        void Render_TrackPanel();
        void Render_KeyPanel();

        void Sync_TargetNameBuffer();
        void Set_CurrentFrame(int32 frame);
        void Refresh_StaticPreview();
        void Handle_TimelineClick(const UI_AnimationTimelineClickResult& clickResult);

    public:
        static Shared<UI_Animation_View> Create(UI_Editor_Manager& editorManager);

    private:
        UI_Editor_Manager& _editorManager;
        UI_AnimationAssetService _assetService = {};
        UI_AnimationTimelineView _timelineView = {};
        char _targetNameBuffer[128] = {};
    };
}
