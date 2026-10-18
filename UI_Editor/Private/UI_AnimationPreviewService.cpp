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

        int32 Clamp_Frame(const UI_AnimationAsset& asset, int32 frame)
        {
            return min(max(frame, asset.startFrame), asset.endFrame);
        }

        float Normalize_KeyframeFrame(const UI_AnimationAsset& asset, float frame)
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

        float Evaluate_Keyframes(const UI_AnimationKeyframeTrack& track, float frame, bool& outHasValue)
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
                const UI_AnimationKeyframe& currentKey = track.keys[index];
                const UI_AnimationKeyframe& nextKey = track.keys[index + 1];
                if (frame < static_cast<float>(currentKey.frame) || frame > static_cast<float>(nextKey.frame))
                    continue;

                const float frameRange = max(1.f, static_cast<float>(nextKey.frame - currentKey.frame));
                const float t = min(max((frame - static_cast<float>(currentKey.frame)) / frameRange, 0.f), 1.f);
                return Lerp_Float(currentKey.value, nextKey.value, Evaluate_AnimationEasing(currentKey.easing, t));
            }

            return track.keys.back().value;
        }

        ImVec2 Evaluate_Vec2Keyframes(const UI_AnimationKeyframeTrack& track, float frame, bool& outHasValue)
        {
            outHasValue = false;
            if (track.keys.empty())
                return ImVec2(0.f, 0.f);

            outHasValue = true;
            if (track.keys.size() == 1)
                return Clamp_AnimationKeyframeTrackVec2Value(track.type, track.keys.front().valueVec2);

            if (frame <= static_cast<float>(track.keys.front().frame))
                return Clamp_AnimationKeyframeTrackVec2Value(track.type, track.keys.front().valueVec2);

            if (frame >= static_cast<float>(track.keys.back().frame))
                return Clamp_AnimationKeyframeTrackVec2Value(track.type, track.keys.back().valueVec2);

            for (size_t index = 0; index + 1 < track.keys.size(); ++index)
            {
                const UI_AnimationKeyframe& currentKey = track.keys[index];
                const UI_AnimationKeyframe& nextKey = track.keys[index + 1];
                if (frame < static_cast<float>(currentKey.frame) || frame > static_cast<float>(nextKey.frame))
                    continue;

                const float frameRange = max(1.f, static_cast<float>(nextKey.frame - currentKey.frame));
                const float t = min(max((frame - static_cast<float>(currentKey.frame)) / frameRange, 0.f), 1.f);
                return Clamp_AnimationKeyframeTrackVec2Value(track.type, Lerp_Vec2(
                    currentKey.valueVec2,
                    nextKey.valueVec2,
                    Evaluate_AnimationEasing(currentKey.easing, t)));
            }

            return Clamp_AnimationKeyframeTrackVec2Value(track.type, track.keys.back().valueVec2);
        }

        ImVec4 Evaluate_ColorKeyframes(const UI_AnimationKeyframeTrack& track, float frame, bool& outHasValue)
        {
            outHasValue = false;
            if (track.keys.empty())
                return ImVec4(255.f, 255.f, 255.f, 255.f);

            outHasValue = true;
            if (track.keys.size() == 1)
                return Clamp_AnimationKeyframeColor(track.keys.front().color);

            if (frame <= static_cast<float>(track.keys.front().frame))
                return Clamp_AnimationKeyframeColor(track.keys.front().color);

            if (frame >= static_cast<float>(track.keys.back().frame))
                return Clamp_AnimationKeyframeColor(track.keys.back().color);

            for (size_t index = 0; index + 1 < track.keys.size(); ++index)
            {
                const UI_AnimationKeyframe& currentKey = track.keys[index];
                const UI_AnimationKeyframe& nextKey = track.keys[index + 1];
                if (frame < static_cast<float>(currentKey.frame) || frame > static_cast<float>(nextKey.frame))
                    continue;

                const float frameRange = max(1.f, static_cast<float>(nextKey.frame - currentKey.frame));
                const float t = min(max((frame - static_cast<float>(currentKey.frame)) / frameRange, 0.f), 1.f);
                return Clamp_AnimationKeyframeColor(Lerp_Color(
                    currentKey.color,
                    nextKey.color,
                    Evaluate_AnimationEasing(currentKey.easing, t)));
            }

            return Clamp_AnimationKeyframeColor(track.keys.back().color);
        }

        const UI_AnimationTargetBinding* Find_KeyframeTarget(const UI_AnimationAsset& asset, const UI_Object& object)
        {
            for (const UI_AnimationTargetBinding& target : asset.targets)
            {
                if (target.targetObjectId == object.id)
                    return &target;
            }

            return nullptr;
        }

        bool Has_KeyframeTarget(const UI_AnimationAsset& asset)
        {
            for (const UI_AnimationTargetBinding& target : asset.targets)
            {
                if (target.targetObjectId != 0)
                    return true;
            }

            return false;
        }

        const vector<UI_AnimationKeyframeTrack>& Get_KeyframeTracks(
            const UI_AnimationAsset& asset,
            const UI_AnimationTargetBinding& target)
        {
            if (Is_PublicAnimation(asset))
                return asset.tracks;

            return target.tracks;
        }

        int32 Get_SequenceTotalFrames(const UI_AnimationSequenceAsset& sequence)
        {
            if (sequence.clips.empty())
                return 0;

            const UI_AnimationSequenceClip& lastClip = sequence.clips.back();
            return lastClip.startFrame + max(1, lastClip.durationFrames);
        }

        const UI_AnimationSequenceClip* Find_SequenceClipAtFrame(
            const UI_AnimationSequenceAsset& sequence,
            float frame,
            float& outLocalSequenceFrame)
        {
            outLocalSequenceFrame = 0.f;
            if (sequence.clips.empty())
                return nullptr;

            const float totalFrames = static_cast<float>(Get_SequenceTotalFrames(sequence));
            const float clampedFrame = min(max(frame, 0.f), totalFrames);
            for (const UI_AnimationSequenceClip& clip : sequence.clips)
            {
                const float clipStartFrame = static_cast<float>(clip.startFrame);
                const float clipEndFrame = static_cast<float>(clip.startFrame + max(1, clip.durationFrames));
                if (clampedFrame >= clipStartFrame && clampedFrame < clipEndFrame)
                {
                    outLocalSequenceFrame = clampedFrame - clipStartFrame;
                    return &clip;
                }
            }

            const UI_AnimationSequenceClip& lastClip = sequence.clips.back();
            outLocalSequenceFrame = static_cast<float>(max(1, lastClip.durationFrames));
            return &lastClip;
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
        else if (nullptr != _sequenceAsset)
        {
            const float endFrame = static_cast<float>(Get_SequenceTotalFrames(*_sequenceAsset));
            if (Get_CurrentSequenceFrameFloat() >= endFrame)
            {
                const float safeFps = static_cast<float>(max(1, _sequenceAsset->fps));
                _time = max(0.f, (endFrame - static_cast<float>(_sequencePreviewStartFrame)) / safeFps);
                _isPlaying = false;
            }
        }
    }

    void UI_AnimationPreviewService::Play(ObjectId objectId)
    {
        if (objectId == 0)
            return;

        _keyframeAsset = nullptr;
        _sequenceAsset = nullptr;
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

    void UI_AnimationPreviewService::Play_KeyframeAsset(const UI_AnimationAsset& asset, int32 startFrame)
    {
        if (!Has_KeyframeTarget(asset))
            return;

        _keyframeAsset = &asset;
        _sequenceAsset = nullptr;
        _targetObjectId = 0;
        _previewStartFrame = Clamp_Frame(asset, startFrame);
        _previewStaticFrame = _previewStartFrame;
        _time = 0.f;
        _isActive = true;
        _isPlaying = true;
    }

    void UI_AnimationPreviewService::Play_SequenceAsset(const UI_AnimationSequenceAsset& sequence, int32 startFrame)
    {
        if (sequence.clips.empty())
            return;

        const int32 totalFrames = Get_SequenceTotalFrames(sequence);
        _keyframeAsset = nullptr;
        _sequenceAsset = &sequence;
        _targetObjectId = 0;
        _sequencePreviewStartFrame = min(max(startFrame, 0), totalFrames);
        _sequencePreviewStaticFrame = _sequencePreviewStartFrame;
        _time = 0.f;
        _isActive = true;
        _isPlaying = true;
    }

    void UI_AnimationPreviewService::Preview_KeyframeAsset(const UI_AnimationAsset& asset, int32 frame)
    {
        if (!Has_KeyframeTarget(asset))
            return;

        _keyframeAsset = &asset;
        _sequenceAsset = nullptr;
        _targetObjectId = 0;
        _previewStartFrame = Clamp_Frame(asset, frame);
        _previewStaticFrame = _previewStartFrame;
        _time = 0.f;
        _isActive = true;
        _isPlaying = false;
    }

    void UI_AnimationPreviewService::Preview_SequenceAsset(const UI_AnimationSequenceAsset& sequence, int32 frame)
    {
        if (sequence.clips.empty())
            return;

        const int32 totalFrames = Get_SequenceTotalFrames(sequence);
        _keyframeAsset = nullptr;
        _sequenceAsset = &sequence;
        _targetObjectId = 0;
        _sequencePreviewStartFrame = min(max(frame, 0), totalFrames);
        _sequencePreviewStaticFrame = _sequencePreviewStartFrame;
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
        _sequenceAsset = nullptr;
        _previewStartFrame = 0;
        _previewStaticFrame = 0;
        _sequencePreviewStartFrame = 0;
        _sequencePreviewStaticFrame = 0;
    }

    void UI_AnimationPreviewService::Restart(ObjectId objectId)
    {
        if (objectId == 0)
            return;

        _keyframeAsset = nullptr;
        _sequenceAsset = nullptr;
        _targetObjectId = objectId;
        _time = 0.f;
        _isActive = true;
        _isPlaying = true;
    }

    int32 UI_AnimationPreviewService::Get_CurrentFrame() const
    {
        if (nullptr == _keyframeAsset)
        {
            if (nullptr == _sequenceAsset)
                return 0;

            const int32 totalFrames = Get_SequenceTotalFrames(*_sequenceAsset);
            return min(max(static_cast<int32>(floorf(Get_CurrentSequenceFrameFloat() + 0.0001f)), 0), totalFrames);
        }

        return Clamp_Frame(*_keyframeAsset, static_cast<int32>(floorf(Get_CurrentKeyframeFrameFloat() + 0.0001f)));
    }

    bool UI_AnimationPreviewService::Sample_Object(const UI_Object& object, UI_AnimationPreviewSample& outSample) const
    {
        if (!_isActive || object.id == 0)
            return false;

        if (nullptr != _sequenceAsset)
            return Sample_SequenceAsset(object, outSample);

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

    float UI_AnimationPreviewService::Get_CurrentSequenceFrameFloat() const
    {
        if (nullptr == _sequenceAsset)
            return 0.f;

        const int32 totalFrames = Get_SequenceTotalFrames(*_sequenceAsset);
        if (!_isPlaying)
            return static_cast<float>(min(max(_sequencePreviewStaticFrame, 0), totalFrames));

        const float safeFps = static_cast<float>(max(1, _sequenceAsset->fps));
        const float frame = static_cast<float>(_sequencePreviewStartFrame) + _time * safeFps;
        return min(max(frame, 0.f), static_cast<float>(totalFrames));
    }

    bool UI_AnimationPreviewService::Sample_KeyframeAsset(const UI_Object& object, UI_AnimationPreviewSample& outSample) const
    {
        if (nullptr == _keyframeAsset)
            return false;

        return Sample_KeyframeAssetAtFrame(*_keyframeAsset, Get_CurrentKeyframeFrameFloat(), object, outSample);
    }

    bool UI_AnimationPreviewService::Sample_KeyframeAssetAtFrame(
        const UI_AnimationAsset& asset,
        float frame,
        const UI_Object& object,
        UI_AnimationPreviewSample& outSample) const
    {
        const UI_AnimationTargetBinding* target = Find_KeyframeTarget(asset, object);
        if (nullptr == target)
            return false;

        frame = min(max(frame, static_cast<float>(asset.startFrame)), static_cast<float>(asset.endFrame));

        UI_AnimationPreviewSample sample = {};
        sample.position = object.position;
        sample.size = object.size;
        sample.scale = object.scale;
        sample.rotationRadians = object.rotationRadians;
        sample.color = object.color;
        sample.alphaMultiplier = object.alphaMultiplier;
        sample.fontSize = object.fontSize;
        sample.patternUvOffset = object.patternUvOffset;
        sample.patternUvScale = object.patternUvScale;
        sample.fillRatio = object.fillRatio;

        bool hasSampledTrack = false;
        const bool useRelativeTracks = Is_PublicAnimation(asset);
        const vector<UI_AnimationKeyframeTrack>& tracks = Get_KeyframeTracks(asset, *target);
        for (const UI_AnimationKeyframeTrack& track : tracks)
        {
            if (!track.enabled)
                continue;

            if (track.type == UI_AnimationKeyframeTrackType::Color)
            {
                bool hasColor = false;
                const ImVec4 color = Evaluate_ColorKeyframes(track, frame, hasColor);
                if (!hasColor)
                    continue;

                sample.color = Convert_AnimationColor255To01(color);
                hasSampledTrack = true;
                continue;
            }

            if (track.type == UI_AnimationKeyframeTrackType::Size)
            {
                bool hasVec2Value = false;
                const ImVec2 value = Evaluate_Vec2Keyframes(track, frame, hasVec2Value);
                if (!hasVec2Value)
                    continue;

                sample.size = useRelativeTracks
                    ? ImVec2(object.size.x + value.x, object.size.y + value.y)
                    : value;
                hasSampledTrack = true;
                continue;
            }

            if (track.type == UI_AnimationKeyframeTrackType::PatternUvOffset ||
                track.type == UI_AnimationKeyframeTrackType::PatternUvScale)
            {
                bool hasVec2Value = false;
                const ImVec2 value = Evaluate_Vec2Keyframes(track, frame, hasVec2Value);
                if (!hasVec2Value)
                    continue;

                if (track.type == UI_AnimationKeyframeTrackType::PatternUvOffset)
                {
                    sample.hasPatternUvOffset = true;
                    sample.patternUvOffset = value;
                }
                else
                {
                    sample.hasPatternUvScale = true;
                    sample.patternUvScale = value;
                }
                hasSampledTrack = true;
                continue;
            }

            bool hasValue = false;
            const float value = Clamp_AnimationKeyframeTrackValue(track.type, Evaluate_Keyframes(track, frame, hasValue));
            if (!hasValue)
                continue;

            hasSampledTrack = true;
            switch (track.type)
            {
            case UI_AnimationKeyframeTrackType::PositionX:
                sample.position.x = useRelativeTracks ? object.position.x + value : value;
                break;
            case UI_AnimationKeyframeTrackType::PositionY:
                sample.position.y = useRelativeTracks ? object.position.y + value : value;
                break;
            case UI_AnimationKeyframeTrackType::ScaleX:
                sample.scale.x = useRelativeTracks ? object.scale.x * value : value;
                break;
            case UI_AnimationKeyframeTrackType::ScaleY:
                sample.scale.y = useRelativeTracks ? object.scale.y * value : value;
                break;
            case UI_AnimationKeyframeTrackType::Rotation:
                sample.rotationRadians = useRelativeTracks ? object.rotationRadians + value : value;
                break;
            case UI_AnimationKeyframeTrackType::Alpha:
                sample.color.w = value;
                break;
            case UI_AnimationKeyframeTrackType::AlphaMultiplier:
                sample.hasAlphaMultiplier = true;
                sample.alphaMultiplier = value;
                break;
            case UI_AnimationKeyframeTrackType::FontSize:
                sample.hasFontSize = true;
                sample.fontSize = value;
                break;
            case UI_AnimationKeyframeTrackType::FillRatio:
                sample.hasFillRatio = true;
                sample.fillRatio = value;
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

    bool UI_AnimationPreviewService::Sample_SequenceAsset(const UI_Object& object, UI_AnimationPreviewSample& outSample) const
    {
        if (nullptr == _sequenceAsset)
            return false;

        float localSequenceFrame = 0.f;
        const UI_AnimationSequenceClip* clip = Find_SequenceClipAtFrame(
            *_sequenceAsset,
            Get_CurrentSequenceFrameFloat(),
            localSequenceFrame);
        if (nullptr == clip)
            return false;

        const float sequenceFps = static_cast<float>(max(1, _sequenceAsset->fps));
        const float assetFps = static_cast<float>(max(1, clip->asset.fps));
        const float assetFrame = static_cast<float>(clip->asset.startFrame) + (localSequenceFrame / sequenceFps) * assetFps;
        return Sample_KeyframeAssetAtFrame(clip->asset, assetFrame, object, outSample);
    }
}
