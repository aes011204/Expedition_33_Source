#include "pch.h"
#include "UI_AnimationAssetService.h"

namespace UIEditor
{
    namespace
    {
        ImVec4 Make_Color255WithAlpha01(const ImVec4& baseColor255, float alpha01)
        {
            ImVec4 color = Clamp_AnimationKeyframeColor(baseColor255);
            color.w = Clamp_AnimationKeyframeTrackValue(UI_AnimationKeyframeTrackType::Alpha, alpha01) * 255.f;
            return color;
        }

        bool Try_GetTextExclusiveCounterpartTrackType(
            UI_AnimationKeyframeTrackType trackType,
            UI_AnimationKeyframeTrackType& outCounterpartTrackType)
        {
            if (trackType == UI_AnimationKeyframeTrackType::FontSize)
            {
                outCounterpartTrackType = UI_AnimationKeyframeTrackType::ScaleX;
                return true;
            }

            if (trackType == UI_AnimationKeyframeTrackType::ScaleX)
            {
                outCounterpartTrackType = UI_AnimationKeyframeTrackType::FontSize;
                return true;
            }

            return false;
        }
    }

    bool UI_AnimationAssetService::Add_Track(UI_AnimationKeyframeTrackType trackType, const UI_Document* document)
    {
        if (trackType == UI_AnimationKeyframeTrackType::Alpha)
            trackType = UI_AnimationKeyframeTrackType::Color;

        if (!Is_PublicAnimationMode() && nullptr == Get_SelectedTarget())
            return false;

        vector<UI_AnimationKeyframeTrack>& tracks = Get_EditableTracks();
        const int32 existingTrackIndex = Find_TrackIndex(tracks, trackType);
        if (existingTrackIndex >= 0)
        {
            _selectedTrackIndex = existingTrackIndex;
            _selectedKeyIndex = -1;
            return false;
        }

        UI_AnimationKeyframeTrackType counterpartTrackType = UI_AnimationKeyframeTrackType::PositionX;
        if (!Is_PublicAnimationMode() &&
            nullptr != document &&
            Try_GetTextExclusiveCounterpartTrackType(trackType, counterpartTrackType))
        {
            const UI_AnimationTargetBinding* target = Get_SelectedTarget();
            const UI_Object* targetObject = nullptr != target ? document->Find_Object(target->targetObjectId) : nullptr;
            if (nullptr != targetObject &&
                targetObject->type == UI_ObjectType::Text &&
                Find_TrackIndex(tracks, counterpartTrackType) >= 0)
            {
                return false;
            }
        }

        UI_AnimationKeyframeTrack track = {};
        track.type = trackType;
        track.enabled = true;
        tracks.push_back(track);
        _selectedTrackIndex = static_cast<int32>(tracks.size()) - 1;
        _selectedKeyIndex = -1;
        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Delete_SelectedTrack()
    {
        if (!Is_PublicAnimationMode() && nullptr == Get_SelectedTarget())
            return false;

        vector<UI_AnimationKeyframeTrack>& tracks = Get_EditableTracks();
        if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(tracks.size()))
            return false;

        tracks.erase(tracks.begin() + _selectedTrackIndex);
        if (_selectedTrackIndex >= static_cast<int32>(tracks.size()))
            _selectedTrackIndex = static_cast<int32>(tracks.size()) - 1;
        _selectedKeyIndex = -1;
        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Add_KeyToSelectedTrack(const UI_Document& document)
    {
        if (nullptr == Get_SelectedTrack())
            return false;

        return Upsert_CurrentValueKey(document, _selectedTrackIndex, true);
    }

    bool UI_AnimationAssetService::Add_CurrentValueKeyToTrack(const UI_Document& document, UI_AnimationKeyframeTrackType trackType)
    {
        const int32 trackIndex = Ensure_Track(trackType);
        if (trackIndex < 0)
            return false;

        return Upsert_CurrentValueKey(document, trackIndex, true);
    }

    bool UI_AnimationAssetService::Add_CurrentValueKeysToTransformTracks(const UI_Document& document)
    {
        if (!Is_PublicAnimationMode() && nullptr == Get_SelectedTarget())
            return false;

        const int32 previousTrackIndex = _selectedTrackIndex;
        int32 fallbackTrackIndex = -1;
        bool changed = false;

        size_t metadataCount = 0;
        const UI_AnimationKeyframeTrackMetadata* metadataList = Get_AnimationKeyframeTrackMetadataList(metadataCount);
        for (size_t metadataIndex = 0; metadataIndex < metadataCount; ++metadataIndex)
        {
            if (!metadataList[metadataIndex].includeInTransformKeySet)
                continue;

            const UI_AnimationKeyframeTrackType trackType = metadataList[metadataIndex].type;
            const int32 trackIndex = Ensure_Track(trackType);
            if (trackIndex < 0)
                continue;

            if (fallbackTrackIndex < 0)
                fallbackTrackIndex = trackIndex;

            changed |= Upsert_CurrentValueKey(document, trackIndex, false);
        }

        int32 preferredTrackIndex = previousTrackIndex;
        const vector<UI_AnimationKeyframeTrack>& tracks = Get_EditableTracks();
        if (preferredTrackIndex < 0 || preferredTrackIndex >= static_cast<int32>(tracks.size()))
            preferredTrackIndex = fallbackTrackIndex;

        if (preferredTrackIndex >= 0 && preferredTrackIndex < static_cast<int32>(tracks.size()))
        {
            _selectedTrackIndex = preferredTrackIndex;
            _selectedKeyIndex = Find_KeyIndexAtFrame(tracks[preferredTrackIndex], _currentFrame);
        }

        return changed;
    }

    bool UI_AnimationAssetService::Copy_SelectedKey()
    {
        UI_AnimationKeyframeTrack* track = Get_SelectedTrack();
        UI_AnimationKeyframe* key = Get_SelectedKey();
        if (nullptr == track || nullptr == key)
            return false;

        _copiedKeyTrackType = track->type;
        _copiedKey = *key;
        _hasCopiedKey = true;
        return true;
    }

    bool UI_AnimationAssetService::Paste_CopiedKey()
    {
        if (!_hasCopiedKey)
            return false;

        if (_copiedKeyTrackType == UI_AnimationKeyframeTrackType::Color)
            return Upsert_KeyColor(_currentFrame, _copiedKey.color, _copiedKey.easing, true);

        if (Is_AnimationKeyframeTrackVec2Value(_copiedKeyTrackType))
            return Upsert_KeyVec2Value(_copiedKeyTrackType, _currentFrame, _copiedKey.valueVec2, _copiedKey.easing, true);

        return Upsert_KeyValue(_copiedKeyTrackType, _currentFrame, _copiedKey.value, _copiedKey.easing, true);
    }

    bool UI_AnimationAssetService::Delete_SelectedKey()
    {
        UI_AnimationKeyframeTrack* track = Get_SelectedTrack();
        if (nullptr == track || _selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int32>(track->keys.size()))
            return false;

        track->keys.erase(track->keys.begin() + _selectedKeyIndex);
        if (_selectedKeyIndex >= static_cast<int32>(track->keys.size()))
            _selectedKeyIndex = static_cast<int32>(track->keys.size()) - 1;
        Mark_AssetDirty();
        return true;
    }

    void UI_AnimationAssetService::Select_Target(int32 targetIndex)
    {
        if (targetIndex < 0 || targetIndex >= static_cast<int32>(_asset.targets.size()))
            return;

        _selectedTargetIndex = targetIndex;
        if (!Is_PublicAnimationMode())
        {
            _selectedTrackIndex = -1;
            _selectedKeyIndex = -1;
        }
    }

    void UI_AnimationAssetService::Select_Track(int32 targetIndex, int32 trackIndex)
    {
        if (Is_PublicAnimationMode())
        {
            if (trackIndex < 0 || trackIndex >= static_cast<int32>(_asset.tracks.size()))
                return;

            if (targetIndex >= 0 && targetIndex < static_cast<int32>(_asset.targets.size()))
                _selectedTargetIndex = targetIndex;

            _selectedTrackIndex = trackIndex;
            _selectedKeyIndex = -1;
            return;
        }

        if (targetIndex < 0 || targetIndex >= static_cast<int32>(_asset.targets.size()))
            return;

        const UI_AnimationTargetBinding& target = _asset.targets[targetIndex];
        if (trackIndex < 0 || trackIndex >= static_cast<int32>(target.tracks.size()))
            return;

        _selectedTargetIndex = targetIndex;
        _selectedTrackIndex = trackIndex;
        _selectedKeyIndex = -1;
    }

    bool UI_AnimationAssetService::Select_Key(int32 targetIndex, int32 trackIndex, int32 keyIndex)
    {
        if (Is_PublicAnimationMode())
        {
            if (trackIndex < 0 || trackIndex >= static_cast<int32>(_asset.tracks.size()))
                return false;

            if (keyIndex < 0 || keyIndex >= static_cast<int32>(_asset.tracks[trackIndex].keys.size()))
                return false;

            if (targetIndex >= 0 && targetIndex < static_cast<int32>(_asset.targets.size()))
                _selectedTargetIndex = targetIndex;

            _selectedTrackIndex = trackIndex;
            _selectedKeyIndex = keyIndex;
            return true;
        }

        if (targetIndex < 0 || targetIndex >= static_cast<int32>(_asset.targets.size()))
            return false;

        const UI_AnimationTargetBinding& target = _asset.targets[targetIndex];
        if (trackIndex < 0 || trackIndex >= static_cast<int32>(target.tracks.size()))
            return false;

        if (keyIndex < 0 || keyIndex >= static_cast<int32>(target.tracks[trackIndex].keys.size()))
            return false;

        _selectedTargetIndex = targetIndex;
        _selectedTrackIndex = trackIndex;
        _selectedKeyIndex = keyIndex;
        return true;
    }

    bool UI_AnimationAssetService::TryMoveSelectedKeyFrame(int32 frame, int32& outFrame)
    {
        UI_AnimationKeyframeTrack* editableTrack = Get_SelectedTrack();
        if (nullptr == editableTrack ||
            _selectedKeyIndex < 0 ||
            _selectedKeyIndex >= static_cast<int32>(editableTrack->keys.size()))
            return false;

        const int32 originalKeyIndex = _selectedKeyIndex;
        const int32 originalFrame = editableTrack->keys[originalKeyIndex].frame;
        const int32 targetFrame = min(max(frame, _asset.startFrame), _asset.endFrame);
        outFrame = originalFrame;

        if (targetFrame == originalFrame)
            return false;

        for (int32 keyIndex = 0; keyIndex < static_cast<int32>(editableTrack->keys.size()); ++keyIndex)
        {
            if (keyIndex != originalKeyIndex && editableTrack->keys[keyIndex].frame == targetFrame)
                return false;
        }

        UI_AnimationKeyframe movingKey = editableTrack->keys[originalKeyIndex];
        movingKey.frame = targetFrame;
        if (editableTrack->type == UI_AnimationKeyframeTrackType::Color)
            movingKey.color = Clamp_AnimationKeyframeColor(movingKey.color);
        else if (Is_AnimationKeyframeTrackVec2Value(editableTrack->type))
            movingKey.valueVec2 = Clamp_AnimationKeyframeTrackVec2Value(editableTrack->type, movingKey.valueVec2);
        else
            movingKey.value = Clamp_AnimationKeyframeTrackValue(editableTrack->type, movingKey.value);
        editableTrack->keys[originalKeyIndex] = movingKey;

        Sort_TrackKeysWithoutMerge(*editableTrack, originalKeyIndex, movingKey);
        outFrame = targetFrame;
        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Set_SelectedKeyValue(float value)
    {
        UI_AnimationKeyframeTrack* editableTrack = Get_SelectedTrack();
        UI_AnimationKeyframe* selectedKey = Get_SelectedKey();
        if (nullptr == editableTrack ||
            nullptr == selectedKey ||
            editableTrack->type == UI_AnimationKeyframeTrackType::Color ||
            Is_AnimationKeyframeTrackVec2Value(editableTrack->type))
        {
            return false;
        }

        selectedKey->value = Clamp_AnimationKeyframeTrackValue(editableTrack->type, value);
        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Set_SelectedKeyVec2Value(const ImVec2& value)
    {
        UI_AnimationKeyframeTrack* editableTrack = Get_SelectedTrack();
        UI_AnimationKeyframe* selectedKey = Get_SelectedKey();
        if (nullptr == editableTrack ||
            nullptr == selectedKey ||
            !Is_AnimationKeyframeTrackVec2Value(editableTrack->type))
        {
            return false;
        }

        selectedKey->valueVec2 = Clamp_AnimationKeyframeTrackVec2Value(editableTrack->type, value);
        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Set_SelectedKeyColor(const ImVec4& color)
    {
        UI_AnimationKeyframeTrack* editableTrack = Get_SelectedTrack();
        UI_AnimationKeyframe* selectedKey = Get_SelectedKey();
        if (nullptr == editableTrack || nullptr == selectedKey || editableTrack->type != UI_AnimationKeyframeTrackType::Color)
            return false;

        selectedKey->color = Clamp_AnimationKeyframeColor(color);
        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Set_SelectedTrackEasing(UI_AnimationEasing easing, bool applyToKeys)
    {
        UI_AnimationKeyframeTrack* editableTrack = Get_SelectedTrack();
        if (nullptr == editableTrack)
            return false;

        editableTrack->easing = easing;
        if (applyToKeys)
        {
            for (UI_AnimationKeyframe& key : editableTrack->keys)
                key.easing = easing;
        }

        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Set_SelectedKeyEasing(UI_AnimationEasing easing)
    {
        UI_AnimationKeyframe* selectedKey = Get_SelectedKey();
        if (nullptr == selectedKey)
            return false;

        selectedKey->easing = easing;
        Mark_AssetDirty();
        return true;
    }

    int32 UI_AnimationAssetService::Find_TrackIndex(const UI_AnimationTargetBinding& target, UI_AnimationKeyframeTrackType trackType) const
    {
        return Find_TrackIndex(target.tracks, trackType);
    }

    int32 UI_AnimationAssetService::Find_TrackIndex(const vector<UI_AnimationKeyframeTrack>& tracks, UI_AnimationKeyframeTrackType trackType) const
    {
        for (int32 index = 0; index < static_cast<int32>(tracks.size()); ++index)
        {
            if (tracks[index].type == trackType)
                return index;
        }

        return -1;
    }

    int32 UI_AnimationAssetService::Find_KeyIndexAtFrame(const UI_AnimationKeyframeTrack& track, int32 frame) const
    {
        for (int32 index = 0; index < static_cast<int32>(track.keys.size()); ++index)
        {
            if (track.keys[index].frame == frame)
                return index;
        }

        return -1;
    }

    int32 UI_AnimationAssetService::Ensure_Track(UI_AnimationKeyframeTrackType trackType)
    {
        if (trackType == UI_AnimationKeyframeTrackType::Alpha)
            trackType = UI_AnimationKeyframeTrackType::Color;

        if (!Is_PublicAnimationMode() && nullptr == Get_SelectedTarget())
            return -1;

        vector<UI_AnimationKeyframeTrack>& tracks = Get_EditableTracks();
        const int32 existingTrackIndex = Find_TrackIndex(tracks, trackType);
        if (existingTrackIndex >= 0)
            return existingTrackIndex;

        UI_AnimationKeyframeTrack track = {};
        track.type = trackType;
        track.enabled = true;
        tracks.push_back(track);
        Mark_AssetDirty();
        return static_cast<int32>(tracks.size()) - 1;
    }

    vector<UI_AnimationKeyframeTrack>& UI_AnimationAssetService::Get_EditableTracks()
    {
        if (Is_PublicAnimationMode())
            return _asset.tracks;

        UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr != target)
            return target->tracks;

        return _asset.tracks;
    }

    const vector<UI_AnimationKeyframeTrack>& UI_AnimationAssetService::Get_EditableTracks() const
    {
        if (Is_PublicAnimationMode())
            return _asset.tracks;

        const UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr != target)
            return target->tracks;

        return _asset.tracks;
    }

    bool UI_AnimationAssetService::Upsert_CurrentValueKey(const UI_Document& document, int32 trackIndex, bool selectKey)
    {
        if (!Is_PublicAnimationMode() && nullptr == Get_SelectedTarget())
            return false;

        vector<UI_AnimationKeyframeTrack>& tracks = Get_EditableTracks();
        if (trackIndex < 0 || trackIndex >= static_cast<int32>(tracks.size()))
            return false;

        UI_AnimationKeyframeTrack& track = tracks[trackIndex];
        if (track.type == UI_AnimationKeyframeTrackType::Color)
            return Upsert_KeyColor(_currentFrame, Get_TargetCurrentColor255(document), track.easing, selectKey);

        if (Is_AnimationKeyframeTrackVec2Value(track.type))
            return Upsert_KeyVec2Value(track.type, _currentFrame, Get_TargetCurrentVec2Value(document, track.type), track.easing, selectKey);

        const float value = Clamp_AnimationKeyframeTrackValue(track.type, Get_TargetCurrentValue(document, track.type));
        for (int32 index = 0; index < static_cast<int32>(track.keys.size()); ++index)
        {
            if (track.keys[index].frame == _currentFrame)
            {
                track.keys[index].value = value;
                if (selectKey)
                {
                    _selectedTrackIndex = trackIndex;
                    _selectedKeyIndex = index;
                }
                Mark_AssetDirty();
                return true;
            }
        }

        UI_AnimationKeyframe key = {};
        key.frame = _currentFrame;
        key.value = value;
        key.easing = track.easing;
        track.keys.push_back(key);

        const int32 previousSelectedTrackIndex = _selectedTrackIndex;
        const int32 previousSelectedKeyIndex = _selectedKeyIndex;
        Sort_TrackKeys(track);

        if (selectKey)
        {
            _selectedTrackIndex = trackIndex;
            _selectedKeyIndex = Find_KeyIndexAtFrame(track, _currentFrame);
        }
        else
        {
            _selectedTrackIndex = previousSelectedTrackIndex;
            _selectedKeyIndex = previousSelectedKeyIndex;
        }

        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Upsert_KeyVec2Value(
        UI_AnimationKeyframeTrackType trackType,
        int32 frame,
        const ImVec2& value,
        UI_AnimationEasing easing,
        bool selectKey)
    {
        const int32 trackIndex = Ensure_Track(trackType);
        if (!Is_PublicAnimationMode() && nullptr == Get_SelectedTarget())
            return false;

        vector<UI_AnimationKeyframeTrack>& tracks = Get_EditableTracks();
        if (trackIndex < 0 || trackIndex >= static_cast<int32>(tracks.size()))
            return false;

        UI_AnimationKeyframeTrack& track = tracks[trackIndex];
        if (!Is_AnimationKeyframeTrackVec2Value(track.type))
            return false;

        track.easing = easing;

        const int32 targetFrame = min(max(frame, _asset.startFrame), _asset.endFrame);
        const ImVec2 targetValue = Clamp_AnimationKeyframeTrackVec2Value(track.type, value);
        for (int32 index = 0; index < static_cast<int32>(track.keys.size()); ++index)
        {
            if (track.keys[index].frame == targetFrame)
            {
                track.keys[index].valueVec2 = targetValue;
                track.keys[index].easing = easing;
                if (selectKey)
                {
                    _selectedTrackIndex = trackIndex;
                    _selectedKeyIndex = index;
                }
                Mark_AssetDirty();
                return true;
            }
        }

        UI_AnimationKeyframe key = {};
        key.frame = targetFrame;
        key.valueVec2 = targetValue;
        key.easing = easing;
        track.keys.push_back(key);

        const int32 previousSelectedTrackIndex = _selectedTrackIndex;
        const int32 previousSelectedKeyIndex = _selectedKeyIndex;
        Sort_TrackKeys(track);

        if (selectKey)
        {
            _selectedTrackIndex = trackIndex;
            _selectedKeyIndex = Find_KeyIndexAtFrame(track, targetFrame);
        }
        else
        {
            _selectedTrackIndex = previousSelectedTrackIndex;
            _selectedKeyIndex = previousSelectedKeyIndex;
        }

        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Upsert_KeyValue(UI_AnimationKeyframeTrackType trackType, int32 frame, float value, UI_AnimationEasing easing, bool selectKey)
    {
        if (trackType == UI_AnimationKeyframeTrackType::Alpha)
        {
            const ImVec4 color = Make_Color255WithAlpha01(ImVec4(255.f, 255.f, 255.f, 255.f), value);
            return Upsert_KeyColor(frame, color, easing, selectKey);
        }

        const int32 trackIndex = Ensure_Track(trackType);
        if (!Is_PublicAnimationMode() && nullptr == Get_SelectedTarget())
            return false;

        vector<UI_AnimationKeyframeTrack>& tracks = Get_EditableTracks();
        if (trackIndex < 0 || trackIndex >= static_cast<int32>(tracks.size()))
            return false;

        UI_AnimationKeyframeTrack& track = tracks[trackIndex];
        track.easing = easing;

        const int32 targetFrame = min(max(frame, _asset.startFrame), _asset.endFrame);
        const float targetValue = Clamp_AnimationKeyframeTrackValue(track.type, value);
        for (int32 index = 0; index < static_cast<int32>(track.keys.size()); ++index)
        {
            if (track.keys[index].frame == targetFrame)
            {
                track.keys[index].value = targetValue;
                track.keys[index].easing = easing;
                if (selectKey)
                {
                    _selectedTrackIndex = trackIndex;
                    _selectedKeyIndex = index;
                }
                Mark_AssetDirty();
                return true;
            }
        }

        UI_AnimationKeyframe key = {};
        key.frame = targetFrame;
        key.value = targetValue;
        key.easing = easing;
        track.keys.push_back(key);

        const int32 previousSelectedTrackIndex = _selectedTrackIndex;
        const int32 previousSelectedKeyIndex = _selectedKeyIndex;
        Sort_TrackKeys(track);

        if (selectKey)
        {
            _selectedTrackIndex = trackIndex;
            _selectedKeyIndex = Find_KeyIndexAtFrame(track, targetFrame);
        }
        else
        {
            _selectedTrackIndex = previousSelectedTrackIndex;
            _selectedKeyIndex = previousSelectedKeyIndex;
        }

        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Upsert_KeyColor(int32 frame, const ImVec4& color, UI_AnimationEasing easing, bool selectKey)
    {
        const int32 trackIndex = Ensure_Track(UI_AnimationKeyframeTrackType::Color);
        if (!Is_PublicAnimationMode() && nullptr == Get_SelectedTarget())
            return false;

        vector<UI_AnimationKeyframeTrack>& tracks = Get_EditableTracks();
        if (trackIndex < 0 || trackIndex >= static_cast<int32>(tracks.size()))
            return false;

        UI_AnimationKeyframeTrack& track = tracks[trackIndex];
        track.easing = easing;

        const int32 targetFrame = min(max(frame, _asset.startFrame), _asset.endFrame);
        const ImVec4 targetColor = Clamp_AnimationKeyframeColor(color);
        for (int32 index = 0; index < static_cast<int32>(track.keys.size()); ++index)
        {
            if (track.keys[index].frame == targetFrame)
            {
                track.keys[index].color = targetColor;
                track.keys[index].easing = easing;
                if (selectKey)
                {
                    _selectedTrackIndex = trackIndex;
                    _selectedKeyIndex = index;
                }
                Mark_AssetDirty();
                return true;
            }
        }

        UI_AnimationKeyframe key = {};
        key.frame = targetFrame;
        key.color = targetColor;
        key.easing = easing;
        track.keys.push_back(key);

        const int32 previousSelectedTrackIndex = _selectedTrackIndex;
        const int32 previousSelectedKeyIndex = _selectedKeyIndex;
        Sort_TrackKeys(track);

        if (selectKey)
        {
            _selectedTrackIndex = trackIndex;
            _selectedKeyIndex = Find_KeyIndexAtFrame(track, targetFrame);
        }
        else
        {
            _selectedTrackIndex = previousSelectedTrackIndex;
            _selectedKeyIndex = previousSelectedKeyIndex;
        }

        Mark_AssetDirty();
        return true;
    }

    void UI_AnimationAssetService::Sort_TrackKeysWithoutMerge(
        UI_AnimationKeyframeTrack& track,
        int32 movingKeyOriginalIndex,
        const UI_AnimationKeyframe& movingKey)
    {
        for (UI_AnimationKeyframe& key : track.keys)
        {
            key.frame = min(max(key.frame, _asset.startFrame), _asset.endFrame);
            if (track.type == UI_AnimationKeyframeTrackType::Color)
                key.color = Clamp_AnimationKeyframeColor(key.color);
            else if (Is_AnimationKeyframeTrackVec2Value(track.type))
                key.valueVec2 = Clamp_AnimationKeyframeTrackVec2Value(track.type, key.valueVec2);
            else
                key.value = Clamp_AnimationKeyframeTrackValue(track.type, key.value);
        }

        stable_sort(
            track.keys.begin(),
            track.keys.end(),
            [](const UI_AnimationKeyframe& lhs, const UI_AnimationKeyframe& rhs)
            {
                return lhs.frame < rhs.frame;
            });

        _selectedKeyIndex = -1;
        for (int32 keyIndex = 0; keyIndex < static_cast<int32>(track.keys.size()); ++keyIndex)
        {
            if (track.keys[keyIndex].frame == movingKey.frame)
            {
                _selectedKeyIndex = keyIndex;
                break;
            }
        }

        if (_selectedKeyIndex < 0 &&
            movingKeyOriginalIndex >= 0 &&
            movingKeyOriginalIndex < static_cast<int32>(track.keys.size()))
        {
            _selectedKeyIndex = movingKeyOriginalIndex;
        }
    }

    void UI_AnimationAssetService::Sort_TrackKeys(UI_AnimationKeyframeTrack& track)
    {
        stable_sort(
            track.keys.begin(),
            track.keys.end(),
            [](const UI_AnimationKeyframe& lhs, const UI_AnimationKeyframe& rhs)
            {
                return lhs.frame < rhs.frame;
            });

        vector<UI_AnimationKeyframe> mergedKeys;
        mergedKeys.reserve(track.keys.size());
        for (const UI_AnimationKeyframe& key : track.keys)
        {
            UI_AnimationKeyframe normalizedKey = key;
            normalizedKey.frame = min(max(normalizedKey.frame, _asset.startFrame), _asset.endFrame);
            if (track.type == UI_AnimationKeyframeTrackType::Color)
                normalizedKey.color = Clamp_AnimationKeyframeColor(normalizedKey.color);
            else if (Is_AnimationKeyframeTrackVec2Value(track.type))
                normalizedKey.valueVec2 = Clamp_AnimationKeyframeTrackVec2Value(track.type, normalizedKey.valueVec2);
            else
                normalizedKey.value = Clamp_AnimationKeyframeTrackValue(track.type, normalizedKey.value);

            if (!mergedKeys.empty() && mergedKeys.back().frame == normalizedKey.frame)
                mergedKeys.back() = normalizedKey;
            else
                mergedKeys.push_back(normalizedKey);
        }
        track.keys = move(mergedKeys);
    }
}
