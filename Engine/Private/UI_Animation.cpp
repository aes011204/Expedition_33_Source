#include "UI_Animation.h"

namespace
{
    float Lerp_Float(float from, float to, float t)
    {
        return from + (to - from) * t;
    }

    Vec2 Lerp_Vec2(const Vec2& from, const Vec2& to, float t)
    {
        return Vec2(Lerp_Float(from.x, to.x, t), Lerp_Float(from.y, to.y, t));
    }

    Vec4 Lerp_Vec4(const Vec4& from, const Vec4& to, float t)
    {
        return Vec4(
            Lerp_Float(from.x, to.x, t),
            Lerp_Float(from.y, to.y, t),
            Lerp_Float(from.z, to.z, t),
            Lerp_Float(from.w, to.w, t));
    }

    float Evaluate_Easing(UI_AnimationEasing easing, float t)
    {
        t = min(max(t, 0.f), 1.f);

        switch (easing)
        {
        case UI_AnimationEasing::Linear:
            return t;
        case UI_AnimationEasing::EaseIn:
            return t * t;
        case UI_AnimationEasing::EaseOut:
            return 1.f - (1.f - t) * (1.f - t);
        case UI_AnimationEasing::EaseInOut:
        {
            const float inverse = -2.f * t + 2.f;
            return t < 0.5f ? 2.f * t * t : 1.f - inverse * inverse * 0.5f;
        }
        default:
            return t;
        }
    }

    float Clamp_KeyframeTrackValue(UI_KeyframeAnimationTrackType type, float value)
    {
        switch (type)
        {
        case UI_KeyframeAnimationTrackType::ScaleX:
        case UI_KeyframeAnimationTrackType::ScaleY:
            return max(0.01f, value);
        case UI_KeyframeAnimationTrackType::Alpha:
            return min(max(value, 0.f), 1.f);
        default:
            return value;
        }
    }

    float Clamp_ColorChannel255(float value)
    {
        return min(max(value, 0.f), 255.f);
    }

    Vec4 Clamp_KeyframeColor255(const Vec4& color)
    {
        return Vec4(
            Clamp_ColorChannel255(color.x),
            Clamp_ColorChannel255(color.y),
            Clamp_ColorChannel255(color.z),
            Clamp_ColorChannel255(color.w));
    }

    Vec4 Convert_Color255To01(const Vec4& color)
    {
        const Vec4 clampedColor = Clamp_KeyframeColor255(color);
        return Vec4(
            clampedColor.x / 255.f,
            clampedColor.y / 255.f,
            clampedColor.z / 255.f,
            clampedColor.w / 255.f);
    }

    void Merge_Sample(UI_AnimationSample& outSample, const UI_AnimationSample& sample)
    {
        if (sample.hasPosition)
        {
            outSample.hasPosition = true;
            outSample.position = sample.position;
        }

        if (sample.hasScale)
        {
            outSample.hasScale = true;
            outSample.scale = sample.scale;
        }

        if (sample.hasRotation)
        {
            outSample.hasRotation = true;
            outSample.rotationRadians = sample.rotationRadians;
        }

        if (sample.hasColor)
        {
            outSample.hasColor = true;

            if (sample.hasColorTrack)
            {
                outSample.hasColorTrack = true;
                outSample.color = sample.color;
            }

            if (sample.hasFadeTrack)
            {
                outSample.hasFadeTrack = true;
                outSample.color.w = sample.color.w;
            }
        }
    }

    bool Sample_Clip(const UI_AnimationClip& clip, float time, UI_AnimationSample& outSample, bool& outReachedEnd)
    {
        outReachedEnd = false;
        if (!clip.enabled)
            return false;

        const float delay = max(0.f, clip.delay);
        const float safeDuration = max(0.001f, clip.duration);
        float localTime = max(0.f, time - delay);

        if (clip.loop)
        {
            localTime = fmodf(localTime, safeDuration);
        }
        else if (localTime >= safeDuration)
        {
            localTime = safeDuration;
            outReachedEnd = true;
        }

        const float normalizedTime = min(max(localTime / safeDuration, 0.f), 1.f);
        const float easedTime = Evaluate_Easing(clip.easing, normalizedTime);

        UI_AnimationSample sample = {};
        bool hasEnabledTrack = false;
        bool hasFadeTrack = false;
        float fadeAlpha = 1.f;

        for (const UI_AnimationTrack& track : clip.tracks)
        {
            if (!track.enabled)
                continue;

            hasEnabledTrack = true;
            switch (track.type)
            {
            case UI_AnimationTrackType::Fade:
                sample.hasColor = true;
                sample.hasFadeTrack = true;
                hasFadeTrack = true;
                fadeAlpha = min(max(Lerp_Float(track.fromFloat, track.toFloat, easedTime), 0.f), 1.f);
                sample.color.w = fadeAlpha;
                break;
            case UI_AnimationTrackType::Move:
                sample.hasPosition = true;
                sample.position = Lerp_Vec2(track.fromVec2, track.toVec2, easedTime);
                break;
            case UI_AnimationTrackType::Scale:
                sample.hasScale = true;
                sample.scale = Lerp_Vec2(track.fromVec2, track.toVec2, easedTime);
                sample.scale.x = max(0.01f, sample.scale.x);
                sample.scale.y = max(0.01f, sample.scale.y);
                break;
            case UI_AnimationTrackType::Rotate:
                sample.hasRotation = true;
                sample.rotationRadians = Lerp_Float(track.fromFloat, track.toFloat, easedTime);
                break;
            case UI_AnimationTrackType::Color:
                sample.hasColor = true;
                sample.hasColorTrack = true;
                sample.color = Lerp_Vec4(track.fromColor, track.toColor, easedTime);
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

    float Normalize_KeyframeFrame(const UI_KeyframeAnimationAsset& asset, float frame, bool& outReachedEnd)
    {
        outReachedEnd = false;

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

        if (frame >= endFrame)
        {
            outReachedEnd = true;
            return endFrame;
        }

        return min(max(frame, startFrame), endFrame);
    }

    float Evaluate_Keyframes(const UI_KeyframeAnimationTrack& track, float frame, bool& outHasValue)
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
            const UI_KeyframeAnimationKey& currentKey = track.keys[index];
            const UI_KeyframeAnimationKey& nextKey = track.keys[index + 1];

            if (frame < static_cast<float>(currentKey.frame) || frame > static_cast<float>(nextKey.frame))
                continue;

            const float frameRange = max(1.f, static_cast<float>(nextKey.frame - currentKey.frame));
            const float t = min(max((frame - static_cast<float>(currentKey.frame)) / frameRange, 0.f), 1.f);
            return Lerp_Float(currentKey.value, nextKey.value, Evaluate_Easing(currentKey.easing, t));
        }

        return track.keys.back().value;
    }

    float Resolve_RelativeKeyframeTargetValue(
        UI_KeyframeAnimationTrackType type,
        float value,
        const Vec2& basePosition,
        const Vec2& baseScale,
        float baseRotationRadians)
    {
        switch (type)
        {
        case UI_KeyframeAnimationTrackType::PositionX:
            return basePosition.x + value;
        case UI_KeyframeAnimationTrackType::PositionY:
            return basePosition.y + value;
        case UI_KeyframeAnimationTrackType::ScaleX:
            return baseScale.x * value;
        case UI_KeyframeAnimationTrackType::ScaleY:
            return baseScale.y * value;
        case UI_KeyframeAnimationTrackType::Rotation:
            return baseRotationRadians + value;
        default:
            return value;
        }
    }

    float Evaluate_KeyframesFromCurrent(
        const UI_KeyframeAnimationTrack& track,
        float frame,
        float currentValue,
        bool useRelativeTargets,
        const Vec2& basePosition,
        const Vec2& baseScale,
        float baseRotationRadians,
        bool& outHasValue)
    {
        outHasValue = false;
        if (track.keys.empty())
            return currentValue;

        outHasValue = true;
        if (track.keys.size() == 1)
        {
            const float value = track.keys.front().value;
            return useRelativeTargets
                ? Resolve_RelativeKeyframeTargetValue(track.type, value, basePosition, baseScale, baseRotationRadians)
                : value;
        }

        if (frame <= static_cast<float>(track.keys.front().frame))
            return currentValue;

        if (frame >= static_cast<float>(track.keys.back().frame))
        {
            const float value = track.keys.back().value;
            return useRelativeTargets
                ? Resolve_RelativeKeyframeTargetValue(track.type, value, basePosition, baseScale, baseRotationRadians)
                : value;
        }

        for (size_t index = 0; index + 1 < track.keys.size(); ++index)
        {
            const UI_KeyframeAnimationKey& currentKey = track.keys[index];
            const UI_KeyframeAnimationKey& nextKey = track.keys[index + 1];

            if (frame < static_cast<float>(currentKey.frame) || frame > static_cast<float>(nextKey.frame))
                continue;

            const float frameRange = max(1.f, static_cast<float>(nextKey.frame - currentKey.frame));
            const float t = min(max((frame - static_cast<float>(currentKey.frame)) / frameRange, 0.f), 1.f);
            const float currentKeyValue = useRelativeTargets
                ? Resolve_RelativeKeyframeTargetValue(track.type, currentKey.value, basePosition, baseScale, baseRotationRadians)
                : currentKey.value;
            const float nextKeyValue = useRelativeTargets
                ? Resolve_RelativeKeyframeTargetValue(track.type, nextKey.value, basePosition, baseScale, baseRotationRadians)
                : nextKey.value;
            const float fromValue = (0 == index) ? currentValue : currentKeyValue;
            return Lerp_Float(fromValue, nextKeyValue, Evaluate_Easing(currentKey.easing, t));
        }

        const float value = track.keys.back().value;
        return useRelativeTargets
            ? Resolve_RelativeKeyframeTargetValue(track.type, value, basePosition, baseScale, baseRotationRadians)
            : value;
    }

    Vec4 Evaluate_ColorKeyframes(const UI_KeyframeAnimationTrack& track, float frame, bool& outHasValue)
    {
        outHasValue = false;
        if (track.keys.empty())
            return Vec4(255.f, 255.f, 255.f, 255.f);

        outHasValue = true;
        if (track.keys.size() == 1)
            return Clamp_KeyframeColor255(track.keys.front().color);

        if (frame <= static_cast<float>(track.keys.front().frame))
            return Clamp_KeyframeColor255(track.keys.front().color);

        if (frame >= static_cast<float>(track.keys.back().frame))
            return Clamp_KeyframeColor255(track.keys.back().color);

        for (size_t index = 0; index + 1 < track.keys.size(); ++index)
        {
            const UI_KeyframeAnimationKey& currentKey = track.keys[index];
            const UI_KeyframeAnimationKey& nextKey = track.keys[index + 1];

            if (frame < static_cast<float>(currentKey.frame) || frame > static_cast<float>(nextKey.frame))
                continue;

            const float frameRange = max(1.f, static_cast<float>(nextKey.frame - currentKey.frame));
            const float t = min(max((frame - static_cast<float>(currentKey.frame)) / frameRange, 0.f), 1.f);
            return Clamp_KeyframeColor255(Lerp_Vec4(
                currentKey.color,
                nextKey.color,
                Evaluate_Easing(currentKey.easing, t)));
        }

        return Clamp_KeyframeColor255(track.keys.back().color);
    }

    bool Sample_KeyframeAsset(
        const UI_KeyframeAnimationAsset& asset,
        float time,
        const Vec2& basePosition,
        const Vec2& baseScale,
        float baseRotationRadians,
        const Vec2& transitionStartPosition,
        const Vec2& transitionStartScale,
        float transitionStartRotationRadians,
        UI_AnimationSample& outSample,
        bool& outReachedEnd)
    {
        outReachedEnd = false;
        if (!asset.enabled)
            return false;

        const float safeFps = static_cast<float>(max(1, asset.fps));
        bool reachedEnd = false;
        const float frame = Normalize_KeyframeFrame(
            asset,
            static_cast<float>(asset.startFrame) + time * safeFps,
            reachedEnd);

        UI_AnimationSample sample = {};
        sample.position = basePosition;
        sample.scale = baseScale;
        sample.rotationRadians = baseRotationRadians;
        bool hasSampledTrack = false;
        const bool useRelativeTracks = asset.valueMode == UI_KeyframeAnimationValueMode::RelativeTransform;
        const bool useCurrentStart =
            asset.valueMode == UI_KeyframeAnimationValueMode::TransitionFromCurrent ||
            asset.valueMode == UI_KeyframeAnimationValueMode::TransitionFromCurrentRelativeTransform;
        const bool useCurrentRelativeTargets =
            asset.valueMode == UI_KeyframeAnimationValueMode::TransitionFromCurrentRelativeTransform;

        for (const UI_KeyframeAnimationTrack& track : asset.tracks)
        {
            if (!track.enabled)
                continue;

            if (track.type == UI_KeyframeAnimationTrackType::Color)
            {
                bool hasColor = false;
                const Vec4 color = Evaluate_ColorKeyframes(track, frame, hasColor);
                if (!hasColor)
                    continue;

                sample.hasColor = true;
                sample.hasColorTrack = true;
                sample.color = Convert_Color255To01(color);
                hasSampledTrack = true;
                continue;
            }

            bool hasValue = false;
            float currentValue = 0.f;
            switch (track.type)
            {
            case UI_KeyframeAnimationTrackType::PositionX:
                currentValue = transitionStartPosition.x;
                break;
            case UI_KeyframeAnimationTrackType::PositionY:
                currentValue = transitionStartPosition.y;
                break;
            case UI_KeyframeAnimationTrackType::ScaleX:
                currentValue = transitionStartScale.x;
                break;
            case UI_KeyframeAnimationTrackType::ScaleY:
                currentValue = transitionStartScale.y;
                break;
            case UI_KeyframeAnimationTrackType::Rotation:
                currentValue = transitionStartRotationRadians;
                break;
            default:
                break;
            }

            const float rawValue = useCurrentStart
                ? Evaluate_KeyframesFromCurrent(
                    track,
                    frame,
                    currentValue,
                    useCurrentRelativeTargets,
                    basePosition,
                    baseScale,
                    baseRotationRadians,
                    hasValue)
                : Evaluate_Keyframes(track, frame, hasValue);
            const float value = Clamp_KeyframeTrackValue(track.type, rawValue);
            if (!hasValue)
                continue;

            hasSampledTrack = true;
            switch (track.type)
            {
            case UI_KeyframeAnimationTrackType::PositionX:
                sample.hasPosition = true;
                sample.position.x = useRelativeTracks ? basePosition.x + value : value;
                break;
            case UI_KeyframeAnimationTrackType::PositionY:
                sample.hasPosition = true;
                sample.position.y = useRelativeTracks ? basePosition.y + value : value;
                break;
            case UI_KeyframeAnimationTrackType::ScaleX:
                sample.hasScale = true;
                sample.scale.x = useRelativeTracks ? baseScale.x * value : value;
                break;
            case UI_KeyframeAnimationTrackType::ScaleY:
                sample.hasScale = true;
                sample.scale.y = useRelativeTracks ? baseScale.y * value : value;
                break;
            case UI_KeyframeAnimationTrackType::Rotation:
                sample.hasRotation = true;
                sample.rotationRadians = useRelativeTracks ? baseRotationRadians + value : value;
                break;
            case UI_KeyframeAnimationTrackType::Alpha:
                sample.hasColor = true;
                sample.hasFadeTrack = true;
                sample.color.w = value;
                break;
            default:
                break;
            }
        }

        if (!hasSampledTrack)
            return false;

        outReachedEnd = reachedEnd;
        outSample = sample;
        return true;
    }
}

void UI_AnimationPlayer::Set_Clips(const vector<UI_AnimationClip>& clips)
{
    _clips = clips;
    Stop();
}

void UI_AnimationPlayer::Set_KeyframeAssets(const vector<UI_KeyframeAnimationAsset>& assets)
{
    _keyframeAssets = assets;
    Stop();
}

void UI_AnimationPlayer::Add_KeyframeAsset(const UI_KeyframeAnimationAsset& asset)
{
    for (UI_KeyframeAnimationAsset& existingAsset : _keyframeAssets)
    {
        if (existingAsset.name == asset.name)
        {
            existingAsset = asset;
            return;
        }
    }

    _keyframeAssets.push_back(asset);
}

void UI_AnimationPlayer::Set_BaseTransformState(const Vec2& position, const Vec2& scale, float rotationRadians)
{
    _basePosition = position;
    _baseScale = scale;
    _baseRotationRadians = rotationRadians;
    Set_TransitionStartState(position, scale, rotationRadians);
}

void UI_AnimationPlayer::Set_TransitionStartState(const Vec2& position, const Vec2& scale, float rotationRadians)
{
    _transitionStartPosition = position;
    _transitionStartScale = scale;
    _transitionStartRotationRadians = rotationRadians;
}

void UI_AnimationPlayer::Play_OnOpen()
{
    _activeClipIndex = Find_PlayOnOpenClipIndex();
    _activeKeyframeAssetIndex = Find_PlayOnOpenKeyframeAssetIndex();
    _time = 0.f;
    _isPlaying =
        _activeClipIndex != numeric_limits<size_t>::max() ||
        _activeKeyframeAssetIndex != numeric_limits<size_t>::max();
}

void UI_AnimationPlayer::Stop()
{
    _activeClipIndex = numeric_limits<size_t>::max();
    _activeKeyframeAssetIndex = numeric_limits<size_t>::max();
    _time = 0.f;
    _isPlaying = false;
}

bool UI_AnimationPlayer::Play_KeyframeAnimation(const wstring& animationName, bool restart)
{
    const size_t assetIndex = Find_KeyframeAssetIndex(animationName);
    if (assetIndex == numeric_limits<size_t>::max())
        return false; // 이름이 없으면 실패 반환

    if (!restart && _isPlaying && _activeKeyframeAssetIndex == assetIndex)
        return true; // 이미 같은 animation 재생 중이면 유지 가능

    _activeClipIndex = numeric_limits<size_t>::max(); // 수동 선택 재생 시 clip 자동재생은 끈다.
    _activeKeyframeAssetIndex = assetIndex;
    _time = 0.f;
    _isPlaying = true;
    return true;
}

bool UI_AnimationPlayer::Update(float deltaTime, UI_AnimationSample& outSample)
{
    if (!_isPlaying)
        return false;

    _time += max(0.f, deltaTime);

    UI_AnimationSample mergedSample = {};
    bool hasSample = false;

    if (_activeClipIndex < _clips.size())
    {
        UI_AnimationSample clipSample = {};
        bool reachedEnd = false;
        if (Sample_Clip(_clips[_activeClipIndex], _time, clipSample, reachedEnd))
        {
            Merge_Sample(mergedSample, clipSample);
            hasSample = true;
        }

        if (reachedEnd || !_clips[_activeClipIndex].enabled)
            _activeClipIndex = numeric_limits<size_t>::max();
    }

    if (_activeKeyframeAssetIndex < _keyframeAssets.size())
    {
        UI_AnimationSample keyframeSample = {};
        bool reachedEnd = false;
        if (Sample_KeyframeAsset(
            _keyframeAssets[_activeKeyframeAssetIndex],
            _time,
            _basePosition,
            _baseScale,
            _baseRotationRadians,
            _transitionStartPosition,
            _transitionStartScale,
            _transitionStartRotationRadians,
            keyframeSample,
            reachedEnd))
        {
            Merge_Sample(mergedSample, keyframeSample);
            hasSample = true;
        }

        if (reachedEnd || !_keyframeAssets[_activeKeyframeAssetIndex].enabled)
            _activeKeyframeAssetIndex = numeric_limits<size_t>::max();
    }

    _isPlaying =
        _activeClipIndex < _clips.size() ||
        _activeKeyframeAssetIndex < _keyframeAssets.size();

    if (!hasSample)
        return false;

    outSample = mergedSample;
    return true;
}

size_t UI_AnimationPlayer::Find_PlayOnOpenClipIndex() const
{
    for (size_t index = 0; index < _clips.size(); ++index)
    {
        const UI_AnimationClip& clip = _clips[index];
        if (clip.enabled && clip.playOnOpen)
            return index;
    }

    return numeric_limits<size_t>::max();
}

size_t UI_AnimationPlayer::Find_PlayOnOpenKeyframeAssetIndex() const
{
    for (size_t index = 0; index < _keyframeAssets.size(); ++index)
    {
        const UI_KeyframeAnimationAsset& asset = _keyframeAssets[index];
        if (asset.enabled && asset.playOnOpen)
            return index;
    }

    return numeric_limits<size_t>::max();
}

size_t UI_AnimationPlayer::Find_KeyframeAssetIndex(const wstring& animationName) const
{
    for (size_t index = 0; index < _keyframeAssets.size(); ++index)
    {
        if (_keyframeAssets[index].name == animationName)
            return index;
    }

    return numeric_limits<size_t>::max();
}
