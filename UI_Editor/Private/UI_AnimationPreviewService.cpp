#include "pch.h"
#include "UI_AnimationPreviewService.h"

namespace UIEditor
{
    namespace
    {
        float Lerp_Float(float from, float to, float t)
        {
            return from + (to - from) * t;
        }

        ImVec2 Lerp_Vec2(const ImVec2& from, const ImVec2& to, float t)
        {
            return ImVec2(Lerp_Float(from.x, to.x, t), Lerp_Float(from.y, to.y, t));
        }

        ImVec4 Lerp_Color(const ImVec4& from, const ImVec4& to, float t)
        {
            return ImVec4(
                Lerp_Float(from.x, to.x, t),
                Lerp_Float(from.y, to.y, t),
                Lerp_Float(from.z, to.z, t),
                Lerp_Float(from.w, to.w, t));
        }

        int32 Clamp_Frame(const UIAnimationAsset& asset, int32 frame)
        {
            return min(max(frame, asset.startFrame), asset.endFrame);
        }

        float Normalize_KeyframeFrame(const UIAnimationAsset& asset, float frame)
        {
            const float startFrame = static_cast<float>(asset.startFrame);
            const float endFrame = static_cast<float>(asset.endFrame);
            const float durationFrames = max(1.f, endFrame - startFrame);

            if (asset.loop)
            {
                float localFrame = fmodf(frame - startFrame, durationFrames);
                if (localFrame < 0.f)
                    localFrame += durationFrames;

                return startFrame + localFrame;
            }

            return min(max(frame, startFrame), endFrame);
        }

        float Evaluate_Keyframes(const UIAnimationTrack& track, float frame, bool& outHasValue)
        {
            outHasValue = false;
            if (track.keys.empty())
                return 0.f;

            outHasValue = true;
            if (track.keys.size() == 1)
                return track.keys.front().value;

            if (frame <= static_cast<float>(track.keys.front().frame))
                return track.keys.front().value;

            if (frame >= static_cast<float>(track.keys.back().frame))
                return track.keys.back().value;

            for (size_t index = 0; index + 1 < track.keys.size(); ++index)
            {
                const UIAnimationKeyframe& currentKey = track.keys[index];
                const UIAnimationKeyframe& nextKey = track.keys[index + 1];
                if (frame < static_cast<float>(currentKey.frame) || frame > static_cast<float>(nextKey.frame))
                    continue;

                const float frameRange = max(1.f, static_cast<float>(nextKey.frame - currentKey.frame));
                const float t = min(max((frame - static_cast<float>(currentKey.frame)) / frameRange, 0.f), 1.f);
                return Lerp_Float(currentKey.value, nextKey.value, t);
            }

            return track.keys.back().value;
        }
    }

    void UI_AnimationPreviewService::Update(float deltaTime)
    {
        if (!_isActive || !_isPlaying)
            return;

        _time += max(0.f, deltaTime);

        if (nullptr != _keyframeAsset && !_keyframeAsset->loop)
        {
            const float endFrame = static_cast<float>(_keyframeAsset->endFrame);
            if (Get_CurrentKeyframeFrameFloat() >= endFrame)
            {
                const float safeFps = static_cast<float>(max(1, _keyframeAsset->fps));
                _time = max(0.f, (endFrame - static_cast<float>(_previewStartFrame)) / safeFps);
                _isPlaying = false;
            }
        }
    }

    void UI_AnimationPreviewService::Play(ObjectId objectId)
    {
        if (objectId == 0)
            return;

        _keyframeAsset = nullptr;
        if (_isActive && _targetObjectId == objectId)
        {
            _isPlaying = true;
            return;
        }

        _targetObjectId = objectId;
        _time = 0.f;
        _isActive = true;
        _isPlaying = true;
    }

    void UI_AnimationPreviewService::Play_KeyframeAsset(const UIAnimationAsset& asset, int32 startFrame)
    {
        if (asset.targetObjectId == 0)
            return;

        _keyframeAsset = &asset;
        _targetObjectId = asset.targetObjectId;
        _previewStartFrame = Clamp_Frame(asset, startFrame);
        _previewStaticFrame = _previewStartFrame;
        _time = 0.f;
        _isActive = true;
        _isPlaying = true;
    }

    void UI_AnimationPreviewService::Preview_KeyframeAsset(const UIAnimationAsset& asset, int32 frame)
    {
        if (asset.targetObjectId == 0)
            return;

        _keyframeAsset = &asset;
        _targetObjectId = asset.targetObjectId;
        _previewStartFrame = Clamp_Frame(asset, frame);
        _previewStaticFrame = _previewStartFrame;
        _time = 0.f;
        _isActive = true;
        _isPlaying = false;
    }

    void UI_AnimationPreviewService::Pause()
    {
        if (!_isActive)
            return;

        _isPlaying = false;
    }

    void UI_AnimationPreviewService::Stop()
    {
        _isActive = false;
        _isPlaying = false;
        _targetObjectId = 0;
        _time = 0.f;
        _keyframeAsset = nullptr;
        _previewStartFrame = 0;
        _previewStaticFrame = 0;
    }

    void UI_AnimationPreviewService::Restart(ObjectId objectId)
    {
        if (objectId == 0)
            return;

        _keyframeAsset = nullptr;
        _targetObjectId = objectId;
        _time = 0.f;
        _isActive = true;
        _isPlaying = true;
    }

    int32 UI_AnimationPreviewService::Get_CurrentFrame() const
    {
        if (nullptr == _keyframeAsset)
            return 0;

        return Clamp_Frame(*_keyframeAsset, static_cast<int32>(floorf(Get_CurrentKeyframeFrameFloat() + 0.0001f)));
    }

    bool UI_AnimationPreviewService::Sample_Object(const UI_Object& object, UI_AnimationPreviewSample& outSample) const
    {
        if (!_isActive || object.id == 0)
            return false;

        if (nullptr != _keyframeAsset)
            return Sample_KeyframeAsset(object, outSample);

        if (object.id != _targetObjectId)
            return false;

        const UI_AnimationClip* clip = Find_Clip(object);
        if (nullptr == clip)
            return false;

        const float safeDuration = max(0.001f, clip->duration);
        const float delay = max(0.f, clip->delay);
        float localTime = max(0.f, _time - delay);
        if (clip->loop)
            localTime = fmodf(localTime, safeDuration);
        else
            localTime = min(localTime, safeDuration);

        const float normalizedTime = min(max(localTime / safeDuration, 0.f), 1.f);
        const float easedTime = Evaluate_AnimationEasing(clip->easing, normalizedTime);

        UI_AnimationPreviewSample sample = {};
        sample.position = object.position;
        sample.scale = object.scale;
        sample.rotationRadians = object.rotationRadians;
        sample.color = object.color;

        bool hasEnabledTrack = false;
        bool hasFadeTrack = false;
        float fadeAlpha = object.color.w;
        for (const UI_AnimationTrack& track : clip->tracks)
        {
            if (!track.enabled)
                continue;

            hasEnabledTrack = true;
            switch (track.type)
            {
            case UI_AnimationTrackType::Fade:
                hasFadeTrack = true;
                fadeAlpha = min(max(Lerp_Float(track.fromFloat, track.toFloat, easedTime), 0.f), 1.f);
                sample.color.w = fadeAlpha;
                break;
            case UI_AnimationTrackType::Move:
                sample.position = Lerp_Vec2(track.fromVec2, track.toVec2, easedTime);
                break;
            case UI_AnimationTrackType::Scale:
                sample.scale = Lerp_Vec2(track.fromVec2, track.toVec2, easedTime);
                sample.scale.x = max(0.01f, sample.scale.x);
                sample.scale.y = max(0.01f, sample.scale.y);
                break;
            case UI_AnimationTrackType::Rotate:
                sample.rotationRadians = Lerp_Float(track.fromFloat, track.toFloat, easedTime);
                break;
            case UI_AnimationTrackType::Color:
                sample.color = Lerp_Color(track.fromColor, track.toColor, easedTime);
                sample.color.x = min(max(sample.color.x, 0.f), 1.f);
                sample.color.y = min(max(sample.color.y, 0.f), 1.f);
                sample.color.z = min(max(sample.color.z, 0.f), 1.f);
                sample.color.w = min(max(sample.color.w, 0.f), 1.f);
                break;
            default:
                break;
            }
        }

        if (hasFadeTrack)
            sample.color.w = fadeAlpha;

        if (!hasEnabledTrack)
            return false;

        outSample = sample;
        return true;
    }

    const UI_AnimationClip* UI_AnimationPreviewService::Find_Clip(const UI_Object& object) const
    {
        for (const UI_AnimationClip& clip : object.animationClips)
        {
            if (clip.enabled)
                return &clip;
        }

        return nullptr;
    }

    float UI_AnimationPreviewService::Get_CurrentKeyframeFrameFloat() const
    {
        if (nullptr == _keyframeAsset)
            return 0.f;

        if (!_isPlaying)
            return static_cast<float>(_previewStaticFrame);

        const float safeFps = static_cast<float>(max(1, _keyframeAsset->fps));
        const float frame = static_cast<float>(_previewStartFrame) + _time * safeFps;
        return Normalize_KeyframeFrame(*_keyframeAsset, frame);
    }

    bool UI_AnimationPreviewService::Sample_KeyframeAsset(const UI_Object& object, UI_AnimationPreviewSample& outSample) const
    {
        if (nullptr == _keyframeAsset || object.id != _keyframeAsset->targetObjectId)
            return false;

        const float frame = Get_CurrentKeyframeFrameFloat();

        UI_AnimationPreviewSample sample = {};
        sample.position = object.position;
        sample.scale = object.scale;
        sample.rotationRadians = object.rotationRadians;
        sample.color = object.color;

        bool hasSampledTrack = false;
        for (const UIAnimationTrack& track : _keyframeAsset->tracks)
        {
            if (!track.enabled)
                continue;

            bool hasValue = false;
            const float value = Clamp_UIAnimationTrackValue(track.type, Evaluate_Keyframes(track, frame, hasValue));
            if (!hasValue)
                continue;

            hasSampledTrack = true;
            switch (track.type)
            {
            case UIAnimationTrackType::PositionX:
                sample.position.x = value;
                break;
            case UIAnimationTrackType::PositionY:
                sample.position.y = value;
                break;
            case UIAnimationTrackType::ScaleX:
                sample.scale.x = value;
                break;
            case UIAnimationTrackType::ScaleY:
                sample.scale.y = value;
                break;
            case UIAnimationTrackType::Rotation:
                sample.rotationRadians = value;
                break;
            case UIAnimationTrackType::Alpha:
                sample.color.w = value;
                break;
            default:
                break;
            }
        }

        if (!hasSampledTrack)
            return false;

        outSample = sample;
        return true;
    }
}
