#include "pch.h"
#include "UI_Animation_View.h"

#include "UI_Editor_Manager.h"

namespace UIEditor
{
    namespace
    {
        const char* const kEditorModeNames[] = {
            "Keyframe Animation",
            "State Animation",
            "Sequence Preview",
        };
    }

    UI_Animation_View::UI_Animation_View(UI_Editor_Manager& editorManager)
        : UI_Editor_Window{ L"Animation" }
        , _editorManager{ editorManager }
    {
        Sync_AssetNameBuffer();
        Sync_StateAssetNameBuffer();
        Sync_StateEventPathBuffers();
        Sync_TargetNameBuffer();
    }

    void UI_Animation_View::Render()
    {
        if (!Is_Open())
        {
            _isFocused = false;
            _isHovered = false;
            return;
        }

        bool isOpen = Is_Open();
        const string windowName = String::ToString(Get_WindowName());
        _isFocused = false;
        _isHovered = false;

        if (ImGui::Begin(windowName.c_str(), &isOpen))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
            Handle_Shortcuts();

            int32 editorModeIndex = static_cast<int32>(_editorMode);
            ImGui::SetNextItemWidth(190.f);
            if (ImGui::Combo("Editor Mode", &editorModeIndex, kEditorModeNames, static_cast<int32>(size(kEditorModeNames))))
            {
                _editorMode = static_cast<UI_AnimationEditorMode>(editorModeIndex);
                _editorManager.Stop_AnimationPreview();
            }
            ImGui::Separator();

            if (_editorMode == UI_AnimationEditorMode::StateAnimation)
            {
                Render_StateAnimationEditor();
            }
            else if (_editorMode == UI_AnimationEditorMode::SequencePreview)
            {
                Render_SequencePreviewEditor();
            }
            else
            {
                Render_KeyframeAnimationEditor();
            }
        }

        Set_Open(isOpen);
        ImGui::End();
    }

    void UI_Animation_View::Sync_AssetNameBuffer()
    {
        memset(_assetNameBuffer, 0, sizeof(_assetNameBuffer));
        strncpy_s(_assetNameBuffer, _assetService.Get_Asset().name.c_str(), _TRUNCATE);
    }

    void UI_Animation_View::Sync_StateAssetNameBuffer()
    {
        memset(_stateAssetNameBuffer, 0, sizeof(_stateAssetNameBuffer));
        strncpy_s(_stateAssetNameBuffer, _stateAssetService.Get_Asset().name.c_str(), _TRUNCATE);
    }

    void UI_Animation_View::Sync_StateEventPathBuffers()
    {
        const UI_StateAnimationAsset& asset = _stateAssetService.Get_Asset();
        for (int32 index = 0; index < static_cast<int32>(UI_StateAnimationEvent::Count); ++index)
        {
            array<char, 260>& buffer = _stateEventPathBuffers[static_cast<size_t>(index)];
            memset(buffer.data(), 0, buffer.size());
            strncpy_s(buffer.data(), buffer.size(), asset.eventAnimationPaths[static_cast<size_t>(index)].c_str(), _TRUNCATE);
        }
    }

    void UI_Animation_View::Sync_TargetNameBuffer()
    {
        memset(_targetNameBuffer, 0, sizeof(_targetNameBuffer));
        const UI_AnimationTargetBinding* target = _assetService.Get_SelectedTarget();
        if (nullptr != target)
            strncpy_s(_targetNameBuffer, target->targetName.c_str(), _TRUNCATE);
    }

    void UI_Animation_View::Set_CurrentFrame(int32 frame)
    {
        _assetService.Set_CurrentFrame(frame);
        Request_StaticPreviewRefresh();
    }

    void UI_Animation_View::Refresh_TargetBindings()
    {
        _assetService.Resolve_TargetObject(_editorManager.Get_Document(), true);
        Request_StaticPreviewRefresh();
    }

    void UI_Animation_View::Mark_StaticPreviewDirty()
    {
        _staticPreviewDirty = true;
    }

    void UI_Animation_View::Request_StaticPreviewRefresh()
    {
        Mark_StaticPreviewDirty();
        Refresh_StaticPreview();
    }

    void UI_Animation_View::Refresh_StaticPreview()
    {
        _assetService.Resolve_TargetObject(_editorManager.Get_Document());
        if (!_assetService.Is_PreviewEnabled() || !_assetService.Has_AnyResolvedTarget())
        {
            _lastStaticPreviewFrame = INT_MIN;
            return;
        }

        if (_editorManager.Is_AnimationPreviewPlaying())
            return;

        const int32 currentFrame = _assetService.Get_CurrentFrame();
        if (!_staticPreviewDirty &&
            _lastStaticPreviewFrame == currentFrame &&
            _editorManager.Is_AnimationPreviewActive())
        {
            return;
        }

        _editorManager.Preview_AnimationAsset(_assetService.Get_Asset(), currentFrame);
        _staticPreviewDirty = false;
        _lastStaticPreviewFrame = currentFrame;
    }

    void UI_Animation_View::Mark_SequenceStaticPreviewDirty()
    {
        _sequenceStaticPreviewDirty = true;
    }

    void UI_Animation_View::Request_SequenceStaticPreviewRefresh()
    {
        Mark_SequenceStaticPreviewDirty();
        Refresh_SequenceStaticPreview();
    }

    void UI_Animation_View::Refresh_SequenceStaticPreview()
    {
        _sequenceService.Resolve_TargetObjects(_editorManager.Get_Document());
        if (_sequenceService.Is_Empty() || !_sequenceService.Has_AnyResolvedTarget())
        {
            _lastSequencePreviewFrame = INT_MIN;
            return;
        }

        if (_editorManager.Is_AnimationPreviewPlaying())
            return;

        const int32 currentFrame = _sequenceService.Get_CurrentFrame();
        if (!_sequenceStaticPreviewDirty &&
            _lastSequencePreviewFrame == currentFrame &&
            _editorManager.Is_AnimationPreviewActive())
        {
            return;
        }

        _editorManager.Preview_AnimationSequence(_sequenceService.Get_Sequence(), currentFrame);
        _sequenceStaticPreviewDirty = false;
        _lastSequencePreviewFrame = currentFrame;
    }

    void UI_Animation_View::Handle_TimelineClick(const UI_AnimationTimelineClickResult& clickResult)
    {
        if (!clickResult.clicked)
            return;

        bool selectedKey = false;
        if (clickResult.targetIndex >= 0 && clickResult.trackIndex >= 0)
        {
            if (clickResult.hitKey)
                selectedKey = _assetService.Select_Key(clickResult.targetIndex, clickResult.trackIndex, clickResult.keyIndex);
            else
                _assetService.Select_Track(clickResult.targetIndex, clickResult.trackIndex);
            Sync_TargetNameBuffer();
        }
        else if (clickResult.trackIndex >= 0)
        {
            if (clickResult.hitKey)
                selectedKey = _assetService.Select_Key(-1, clickResult.trackIndex, clickResult.keyIndex);
            else
                _assetService.Select_Track(-1, clickResult.trackIndex);
        }
        else if (clickResult.targetIndex >= 0)
        {
            _assetService.Select_Target(clickResult.targetIndex);
            Sync_TargetNameBuffer();
        }

        if (clickResult.draggedKey)
        {
            if (!selectedKey)
                return;

            int32 targetFrame = clickResult.frame;
            if (_assetService.TryMoveSelectedKeyFrame(clickResult.frame, targetFrame))
                Set_CurrentFrame(targetFrame);
            return;
        }

        Set_CurrentFrame(clickResult.frame);
    }

    void UI_Animation_View::Handle_SequenceTimelineClick(const UI_AnimationSequenceTimelineClickResult& clickResult)
    {
        if (!clickResult.clicked)
            return;

        _sequenceService.Select_Clip(clickResult.clipIndex);
        _sequenceService.Set_CurrentFrame(clickResult.frame);
        Request_SequenceStaticPreviewRefresh();
    }

    Shared<UI_Animation_View> UI_Animation_View::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Animation_View>(editorManager);
    }
}
