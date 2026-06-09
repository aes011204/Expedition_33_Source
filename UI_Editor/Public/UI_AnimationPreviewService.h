#pragma once

#include "UI_Editor_Struct.h"

NS_BEGIN(UIEditor)

struct UI_AnimationPreviewSample
{
    ImVec2 position = ImVec2(0.f, 0.f);
    ImVec2 size = ImVec2(100.f, 100.f);
    ImVec2 scale = ImVec2(1.f, 1.f);
    float rotationRadians = 0.f;
    ImVec4 color = ImVec4(1.f, 1.f, 1.f, 1.f);
    bool hasAlphaMultiplier = false;
    float alphaMultiplier = 1.f;
    bool hasFontSize = false;
    float fontSize = 32.f;
};

class UI_AnimationPreviewService final
{
public:
    UI_AnimationPreviewService() = default;
    ~UI_AnimationPreviewService() = default;

public:
    void Update(float deltaTime);
    void Play(ObjectId objectId);
    void Play_KeyframeAsset(const UI_AnimationAsset& asset, int32 startFrame);
    void Play_SequenceAsset(const UI_AnimationSequenceAsset& sequence, int32 startFrame);
    void Preview_KeyframeAsset(const UI_AnimationAsset& asset, int32 frame);
    void Preview_SequenceAsset(const UI_AnimationSequenceAsset& sequence, int32 frame);
    void Pause();
    void Stop();
    void Restart(ObjectId objectId);

    bool Is_Active() const { return _isActive; }
    bool Is_Playing() const { return _isActive && _isPlaying; }
    ObjectId Get_TargetObjectId() const { return _targetObjectId; }
    float Get_Time() const { return _time; }
    int32 Get_CurrentFrame() const;

    bool Sample_Object(const UI_Object& object, UI_AnimationPreviewSample& outSample) const;

private:
    const UI_AnimationClip* Find_Clip(const UI_Object& object) const;
    float Get_CurrentKeyframeFrameFloat() const;
    float Get_CurrentSequenceFrameFloat() const;
    bool Sample_KeyframeAsset(const UI_Object& object, UI_AnimationPreviewSample& outSample) const;
    bool Sample_KeyframeAssetAtFrame(const UI_AnimationAsset& asset, float frame, const UI_Object& object, UI_AnimationPreviewSample& outSample) const;
    bool Sample_SequenceAsset(const UI_Object& object, UI_AnimationPreviewSample& outSample) const;

private:
    bool _isActive = false;
    bool _isPlaying = false;
    ObjectId _targetObjectId = 0;
    float _time = 0.f;
    const UI_AnimationAsset* _keyframeAsset = nullptr;
    const UI_AnimationSequenceAsset* _sequenceAsset = nullptr;
    int32 _previewStartFrame = 0;
    int32 _previewStaticFrame = 0;
    int32 _sequencePreviewStartFrame = 0;
    int32 _sequencePreviewStaticFrame = 0;
};

NS_END
