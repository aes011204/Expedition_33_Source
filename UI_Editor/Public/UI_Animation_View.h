#pragma once

#include "UI_AnimationAssetService.h"
#include "UI_AnimationTimelineView.h"
#include "UI_StateAnimationAssetService.h"
#include "UI_Editor_Window.h"

namespace UIEditor
{
    class UI_Editor_Manager;

    enum class UI_AnimationEditorMode
    {
        KeyframeAnimation,
        StateAnimation,
    };

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
        void Render_KeyframeAnimationEditor();
        void Render_StateAnimationEditor();
        void Render_StateAnimationToolbar();
        void Render_StateAnimationEvents();
        void Render_TargetSection();
        void Render_PlaybackSection();
        void Render_TimelineSection();
        void Render_TrackPanel();
        void Render_KeyPanel();

        void Sync_AssetNameBuffer();
        void Sync_StateAssetNameBuffer();
        void Sync_StateEventPathBuffers();
        void Sync_TargetNameBuffer();
        void Set_CurrentFrame(int32 frame);
        bool Undo_KeyframeAnimationEdit();
        bool Redo_KeyframeAnimationEdit();
        bool Delete_SelectedAnimationItem();
        bool Undo_StateAnimationEdit();
        bool Redo_StateAnimationEdit();
        void Mark_StaticPreviewDirty();
        void Request_StaticPreviewRefresh();
        void Refresh_StaticPreview();
        void Handle_TimelineClick(const UI_AnimationTimelineClickResult& clickResult);
        void Handle_Shortcuts();

    public:
        static Shared<UI_Animation_View> Create(UI_Editor_Manager& editorManager);

    private:
        UI_Editor_Manager& _editorManager;
        UI_AnimationEditorMode _editorMode = UI_AnimationEditorMode::KeyframeAnimation;
        UI_AnimationAssetService _assetService = {};
        UI_StateAnimationAssetService _stateAssetService = {};
        UI_AnimationTimelineView _timelineView = {};
        char _assetNameBuffer[128] = {};
        char _stateAssetNameBuffer[128] = {};
        array<array<char, 260>, static_cast<size_t>(UI_StateAnimationEvent::Count)> _stateEventPathBuffers = {};
        char _targetNameBuffer[128] = {};
        bool _staticPreviewDirty = true;
        int32 _lastStaticPreviewFrame = INT_MIN;
    };
}
