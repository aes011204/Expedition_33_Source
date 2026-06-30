#include "pch.h"
#include "UI_Animation_View.h"

#include "UI_Editor_Manager.h"

namespace UIEditor
{
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
}
