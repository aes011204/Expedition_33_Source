#include "pch.h"
#include "UI_Editor_Manager.h"

namespace UIEditor
{
    void UI_Editor_Manager::Play_AnimationPreview(ObjectId objectId)
    {
        _animationPreviewService.Play(objectId);
    }

    void UI_Editor_Manager::Play_AnimationAssetPreview(const UI_AnimationAsset& asset, int32 startFrame)
    {
        _animationPreviewService.Play_KeyframeAsset(asset, startFrame);
    }

    void UI_Editor_Manager::Play_AnimationSequencePreview(const UI_AnimationSequenceAsset& sequence, int32 startFrame)
    {
        _animationPreviewService.Play_SequenceAsset(sequence, startFrame);
    }

    void UI_Editor_Manager::Preview_AnimationAsset(const UI_AnimationAsset& asset, int32 frame)
    {
        _animationPreviewService.Preview_KeyframeAsset(asset, frame);
    }

    void UI_Editor_Manager::Preview_AnimationSequence(const UI_AnimationSequenceAsset& sequence, int32 frame)
    {
        _animationPreviewService.Preview_SequenceAsset(sequence, frame);
    }

    void UI_Editor_Manager::Pause_AnimationPreview()
    {
        _animationPreviewService.Pause();
    }

    void UI_Editor_Manager::Stop_AnimationPreview()
    {
        _animationPreviewService.Stop();
    }

    void UI_Editor_Manager::Restart_AnimationPreview(ObjectId objectId)
    {
        _animationPreviewService.Restart(objectId);
    }

    bool UI_Editor_Manager::Is_AnimationPreviewActive() const
    {
        return _animationPreviewService.Is_Active();
    }

    bool UI_Editor_Manager::Is_AnimationPreviewPlaying() const
    {
        return _animationPreviewService.Is_Playing();
    }

    float UI_Editor_Manager::Get_AnimationPreviewTime() const
    {
        return _animationPreviewService.Get_Time();
    }

    int32 UI_Editor_Manager::Get_AnimationPreviewFrame() const
    {
        return _animationPreviewService.Get_CurrentFrame();
    }

    bool UI_Editor_Manager::Sample_AnimationPreview(const UI_Object& object, UI_AnimationPreviewSample& outSample) const
    {
        return _animationPreviewService.Sample_Object(object, outSample);
    }
}
