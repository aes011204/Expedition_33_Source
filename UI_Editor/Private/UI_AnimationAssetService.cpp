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
    }

    UI_AnimationAssetService::UI_AnimationAssetService()
    {
        New_Asset();
    }

    bool UI_AnimationAssetService::Has_AnyResolvedTarget() const
    {
        for (const UI_AnimationTargetBinding& target : _asset.targets)
        {
            if (target.targetObjectId != 0)
                return true;
        }

        return false;
    }

    bool UI_AnimationAssetService::Has_SelectedResolvedTarget() const
    {
        const UI_AnimationTargetBinding* target = Get_SelectedTarget();
        return nullptr != target && target->targetObjectId != 0;
    }

    void UI_AnimationAssetService::New_Asset()
    {
        _asset = {};
        _assetFilePath.clear();
        _assetDirty = false;
        _previewEnabled = true;
        _targetBindingDirty = true;
        _legacyAlphaMigrationPending = false;
        _currentFrame = _asset.startFrame;
        _selectedTargetIndex = -1;
        _selectedTrackIndex = -1;
        _selectedKeyIndex = -1;
        _pendingTrackType = UI_AnimationKeyframeTrackType::PositionX;
        _hasCopiedKey = false;
        _copiedKeyTrackType = UI_AnimationKeyframeTrackType::PositionX;
        _copiedKey = {};
    }

    bool UI_AnimationAssetService::Save_Asset(bool forceSaveAs)
    {
        wstring filePath = _assetFilePath;
        if (forceSaveAs || filePath.empty())
        {
            if (!Show_SaveAssetDialog(filePath))
                return false;
        }

        Ensure_AnimationExtension(filePath);
        Clamp_Asset();
        Normalize_AllTracks();

        if (!UI_AnimationAssetSerializer::Save_ToJsonFile(_asset, filePath))
            return false;

        _assetFilePath = filePath;
        _assetDirty = false;
        return true;
    }

    bool UI_AnimationAssetService::Load_Asset()
    {
        wstring filePath = {};
        if (!Show_LoadAssetDialog(filePath))
            return false;

        UI_AnimationAsset loadedAsset = {};
        if (!UI_AnimationAssetSerializer::Load_FromJsonFile(loadedAsset, filePath))
            return false;

        _asset = move(loadedAsset);
        _assetFilePath = filePath;
        _assetDirty = false;
        _targetBindingDirty = true;
        _legacyAlphaMigrationPending = true;
        _selectedTargetIndex = _asset.targets.empty() ? -1 : 0;
        _selectedTrackIndex = -1;
        if (_selectedTargetIndex >= 0 && !_asset.targets[_selectedTargetIndex].tracks.empty())
            _selectedTrackIndex = 0;
        _selectedKeyIndex = -1;
        _hasCopiedKey = false;
        _copiedKeyTrackType = UI_AnimationKeyframeTrackType::PositionX;
        _copiedKey = {};
        Clamp_Asset();
        Clamp_SelectedIndices();
        _currentFrame = _asset.startFrame;
        return true;
    }

    bool UI_AnimationAssetService::Confirm_DiscardDirtyAsset(const wchar_t* actionName)
    {
        if (!_assetDirty)
            return true;

        const wstring actionText = (nullptr != actionName && L'\0' != actionName[0]) ? actionName : L"continue";
        const wstring message = L"The current UI animation asset has unsaved changes.\n\nSave before you " + actionText + L"?";
        const int result = MessageBoxW(nullptr, message.c_str(), L"UI Animation", MB_ICONQUESTION | MB_YESNOCANCEL);

        if (result == IDCANCEL)
            return false;

        if (result == IDYES)
            return Save_Asset(false);

        return true;
    }

    bool UI_AnimationAssetService::Bind_SelectedUI(const UI_Object* selectedObject)
    {
        if (nullptr == selectedObject)
            return false;

        int32 targetIndex = Find_TargetIndexByObjectId(selectedObject->id);
        if (targetIndex < 0)
            targetIndex = Find_TargetIndexByName(selectedObject->name);

        if (targetIndex >= 0)
        {
            _selectedTargetIndex = targetIndex;
            const bool renamedTarget = _asset.targets[targetIndex].targetName != selectedObject->name;
            _asset.targets[targetIndex].targetName = selectedObject->name;
            _asset.targets[targetIndex].targetObjectId = selectedObject->id;
            _targetBindingDirty = false;
            Clamp_SelectedIndices();
            if (renamedTarget)
                Mark_AssetDirty();
            return true;
        }

        UI_AnimationTargetBinding target = {};
        target.targetName = selectedObject->name;
        target.targetObjectId = selectedObject->id;
        _asset.targets.push_back(move(target));
        _targetBindingDirty = false;
        _selectedTargetIndex = static_cast<int32>(_asset.targets.size()) - 1;
        _selectedTrackIndex = -1;
        _selectedKeyIndex = -1;
        Mark_AssetDirty();
        return true;
    }

    void UI_AnimationAssetService::Clear_Target()
    {
        UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr == target)
            return;

        target->targetName.clear();
        target->targetObjectId = 0;
        _targetBindingDirty = false;
        Mark_AssetDirty();
    }

    void UI_AnimationAssetService::Resolve_TargetObject(const UI_Document& document, bool forceResolve)
    {
        if (!forceResolve && !_targetBindingDirty && !_legacyAlphaMigrationPending)
            return;

        for (UI_AnimationTargetBinding& target : _asset.targets)
        {
            if (target.targetName.empty())
            {
                target.targetObjectId = 0;
                continue;
            }

            const UI_Object* currentTarget = document.Find_Object(target.targetObjectId);
            if (!forceResolve && nullptr != currentTarget && currentTarget->name == target.targetName)
                continue;

            target.targetObjectId = 0;
            for (const UI_Object& object : document.Get_Objects())
            {
                if (object.name == target.targetName)
                {
                    target.targetObjectId = object.id;
                    break;
                }
            }
        }

        _targetBindingDirty = false;

        if (_legacyAlphaMigrationPending)
        {
            Migrate_LegacyAlphaTracks(document);
            _legacyAlphaMigrationPending = false;
        }
    }

    void UI_AnimationAssetService::Set_TargetName(const string& targetName, const UI_Document& document)
    {
        if (nullptr == Get_SelectedTarget())
        {
            UI_AnimationTargetBinding target = {};
            target.targetName = targetName;
            _asset.targets.push_back(move(target));
            _selectedTargetIndex = static_cast<int32>(_asset.targets.size()) - 1;
            _selectedTrackIndex = -1;
            _selectedKeyIndex = -1;
        }
        else
        {
            Get_SelectedTarget()->targetName = targetName;
        }

        _targetBindingDirty = true;
        Resolve_TargetObject(document, true);
        Mark_AssetDirty();
    }

    void UI_AnimationAssetService::Set_AssetName(const string& assetName)
    {
        if (_asset.name == assetName)
            return;

        _asset.name = assetName;
        Mark_AssetDirty();
    }

    void UI_AnimationAssetService::Set_CurrentFrame(int32 frame)
    {
        Clamp_AssetRange();
        _currentFrame = min(max(frame, _asset.startFrame), _asset.endFrame);
    }

    void UI_AnimationAssetService::Clamp_Asset()
    {
        Clamp_AssetRange();

        for (UI_AnimationTargetBinding& target : _asset.targets)
        {
            for (UI_AnimationKeyframeTrack& track : target.tracks)
            {
                for (UI_AnimationKeyframe& key : track.keys)
                {
                    key.frame = min(max(key.frame, _asset.startFrame), _asset.endFrame);
                    if (track.type == UI_AnimationKeyframeTrackType::Color)
                        key.color = Clamp_AnimationKeyframeColor(key.color);
                    else
                        key.value = Clamp_AnimationKeyframeTrackValue(track.type, key.value);
                }
            }
        }

        Clamp_SelectedIndices();
    }

    void UI_AnimationAssetService::Clamp_AssetRange()
    {
        _asset.fps = max(1, _asset.fps);
        _asset.endFrame = max(_asset.startFrame + 1, _asset.endFrame);
        _currentFrame = min(max(_currentFrame, _asset.startFrame), _asset.endFrame);
    }

    void UI_AnimationAssetService::Normalize_AllTracks()
    {
        for (UI_AnimationTargetBinding& target : _asset.targets)
        {
            for (UI_AnimationKeyframeTrack& track : target.tracks)
                Sort_TrackKeys(track);
        }
    }

    UI_AnimationTargetBinding* UI_AnimationAssetService::Get_SelectedTarget()
    {
        if (_selectedTargetIndex < 0 || _selectedTargetIndex >= static_cast<int32>(_asset.targets.size()))
            return nullptr;

        return &_asset.targets[_selectedTargetIndex];
    }

    const UI_AnimationTargetBinding* UI_AnimationAssetService::Get_SelectedTarget() const
    {
        if (_selectedTargetIndex < 0 || _selectedTargetIndex >= static_cast<int32>(_asset.targets.size()))
            return nullptr;

        return &_asset.targets[_selectedTargetIndex];
    }

    UI_AnimationKeyframeTrack* UI_AnimationAssetService::Get_SelectedTrack()
    {
        UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr == target || _selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(target->tracks.size()))
            return nullptr;

        return &target->tracks[_selectedTrackIndex];
    }

    const UI_AnimationKeyframeTrack* UI_AnimationAssetService::Get_SelectedTrack() const
    {
        const UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr == target || _selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(target->tracks.size()))
            return nullptr;

        return &target->tracks[_selectedTrackIndex];
    }

    UI_AnimationKeyframe* UI_AnimationAssetService::Get_SelectedKey()
    {
        UI_AnimationKeyframeTrack* track = Get_SelectedTrack();
        if (nullptr == track || _selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int32>(track->keys.size()))
            return nullptr;

        return &track->keys[_selectedKeyIndex];
    }

    const UI_AnimationKeyframe* UI_AnimationAssetService::Get_SelectedKey() const
    {
        const UI_AnimationKeyframeTrack* track = Get_SelectedTrack();
        if (nullptr == track || _selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int32>(track->keys.size()))
            return nullptr;

        return &track->keys[_selectedKeyIndex];
    }

    float UI_AnimationAssetService::Get_TargetCurrentValue(const UI_Document& document, UI_AnimationKeyframeTrackType trackType) const
    {
        const UI_AnimationTargetBinding* target = Get_SelectedTarget();
        const UI_Object* targetObject = nullptr != target ? document.Find_Object(target->targetObjectId) : nullptr;
        if (nullptr == targetObject)
        {
            if (trackType == UI_AnimationKeyframeTrackType::ScaleX || trackType == UI_AnimationKeyframeTrackType::ScaleY || trackType == UI_AnimationKeyframeTrackType::Alpha)
                return 1.f;

            return 0.f;
        }

        switch (trackType)
        {
        case UI_AnimationKeyframeTrackType::PositionX:
            return targetObject->position.x;
        case UI_AnimationKeyframeTrackType::PositionY:
            return targetObject->position.y;
        case UI_AnimationKeyframeTrackType::ScaleX:
            return targetObject->scale.x;
        case UI_AnimationKeyframeTrackType::ScaleY:
            return targetObject->scale.y;
        case UI_AnimationKeyframeTrackType::Rotation:
            return targetObject->rotationRadians;
        case UI_AnimationKeyframeTrackType::Alpha:
            return targetObject->color.w;
        case UI_AnimationKeyframeTrackType::Color:
            return 0.f;
        default:
            return 0.f;
        }
    }

    ImVec4 UI_AnimationAssetService::Get_TargetCurrentColor255(const UI_Document& document) const
    {
        const UI_AnimationTargetBinding* target = Get_SelectedTarget();
        const UI_Object* targetObject = nullptr != target ? document.Find_Object(target->targetObjectId) : nullptr;
        if (nullptr == targetObject)
            return ImVec4(255.f, 255.f, 255.f, 255.f);

        return Convert_AnimationColor01To255(targetObject->color);
    }

    bool UI_AnimationAssetService::Add_Track(UI_AnimationKeyframeTrackType trackType)
    {
        if (trackType == UI_AnimationKeyframeTrackType::Alpha)
            trackType = UI_AnimationKeyframeTrackType::Color;

        UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr == target)
            return false;

        const int32 existingTrackIndex = Find_TrackIndex(*target, trackType);
        if (existingTrackIndex >= 0)
        {
            _selectedTrackIndex = existingTrackIndex;
            _selectedKeyIndex = -1;
            return false;
        }

        UI_AnimationKeyframeTrack track = {};
        track.type = trackType;
        track.enabled = true;
        target->tracks.push_back(track);
        _selectedTrackIndex = static_cast<int32>(target->tracks.size()) - 1;
        _selectedKeyIndex = -1;
        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Delete_SelectedTrack()
    {
        UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr == target || _selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(target->tracks.size()))
            return false;

        target->tracks.erase(target->tracks.begin() + _selectedTrackIndex);
        if (_selectedTrackIndex >= static_cast<int32>(target->tracks.size()))
            _selectedTrackIndex = static_cast<int32>(target->tracks.size()) - 1;
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
        UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr == target)
            return false;

        const UI_AnimationKeyframeTrackType trackTypes[] = {
            UI_AnimationKeyframeTrackType::PositionX,
            UI_AnimationKeyframeTrackType::PositionY,
            UI_AnimationKeyframeTrackType::ScaleX,
            UI_AnimationKeyframeTrackType::ScaleY,
            UI_AnimationKeyframeTrackType::Rotation,
            UI_AnimationKeyframeTrackType::Color,
        };

        const int32 previousTrackIndex = _selectedTrackIndex;
        int32 fallbackTrackIndex = -1;
        bool changed = false;

        for (UI_AnimationKeyframeTrackType trackType : trackTypes)
        {
            const int32 trackIndex = Ensure_Track(trackType);
            if (trackIndex < 0)
                continue;

            if (fallbackTrackIndex < 0)
                fallbackTrackIndex = trackIndex;

            changed |= Upsert_CurrentValueKey(document, trackIndex, false);
        }

        int32 preferredTrackIndex = previousTrackIndex;
        if (preferredTrackIndex < 0 || preferredTrackIndex >= static_cast<int32>(target->tracks.size()))
            preferredTrackIndex = fallbackTrackIndex;

        if (preferredTrackIndex >= 0 && preferredTrackIndex < static_cast<int32>(target->tracks.size()))
        {
            _selectedTrackIndex = preferredTrackIndex;
            _selectedKeyIndex = Find_KeyIndexAtFrame(target->tracks[preferredTrackIndex], _currentFrame);
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

    bool UI_AnimationAssetService::Remove_SelectedTarget()
    {
        if (_selectedTargetIndex < 0 || _selectedTargetIndex >= static_cast<int32>(_asset.targets.size()))
            return false;

        _asset.targets.erase(_asset.targets.begin() + _selectedTargetIndex);
        if (_selectedTargetIndex >= static_cast<int32>(_asset.targets.size()))
            _selectedTargetIndex = static_cast<int32>(_asset.targets.size()) - 1;

        _selectedTrackIndex = -1;
        _selectedKeyIndex = -1;
        if (nullptr != Get_SelectedTarget() && !Get_SelectedTarget()->tracks.empty())
            _selectedTrackIndex = 0;

        Mark_AssetDirty();
        return true;
    }

    void UI_AnimationAssetService::Select_Target(int32 targetIndex)
    {
        if (targetIndex < 0 || targetIndex >= static_cast<int32>(_asset.targets.size()))
            return;

        _selectedTargetIndex = targetIndex;
        _selectedTrackIndex = -1;
        _selectedKeyIndex = -1;
    }

    void UI_AnimationAssetService::Select_Track(int32 targetIndex, int32 trackIndex)
    {
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
        if (nullptr == editableTrack || nullptr == selectedKey || editableTrack->type == UI_AnimationKeyframeTrackType::Color)
            return false;

        selectedKey->value = Clamp_AnimationKeyframeTrackValue(editableTrack->type, value);
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

    void UI_AnimationAssetService::Mark_AssetDirty()
    {
        _assetDirty = true;
    }

    int32 UI_AnimationAssetService::Find_TargetIndexByName(const string& targetName) const
    {
        if (targetName.empty())
            return -1;

        for (int32 index = 0; index < static_cast<int32>(_asset.targets.size()); ++index)
        {
            if (_asset.targets[index].targetName == targetName)
                return index;
        }

        return -1;
    }

    int32 UI_AnimationAssetService::Find_TargetIndexByObjectId(uint32 targetObjectId) const
    {
        if (targetObjectId == 0)
            return -1;

        for (int32 index = 0; index < static_cast<int32>(_asset.targets.size()); ++index)
        {
            if (_asset.targets[index].targetObjectId == targetObjectId)
                return index;
        }

        return -1;
    }

    int32 UI_AnimationAssetService::Find_TrackIndex(const UI_AnimationTargetBinding& target, UI_AnimationKeyframeTrackType trackType) const
    {
        for (int32 index = 0; index < static_cast<int32>(target.tracks.size()); ++index)
        {
            if (target.tracks[index].type == trackType)
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

        UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr == target)
            return -1;

        const int32 existingTrackIndex = Find_TrackIndex(*target, trackType);
        if (existingTrackIndex >= 0)
            return existingTrackIndex;

        UI_AnimationKeyframeTrack track = {};
        track.type = trackType;
        track.enabled = true;
        target->tracks.push_back(track);
        Mark_AssetDirty();
        return static_cast<int32>(target->tracks.size()) - 1;
    }

    bool UI_AnimationAssetService::Upsert_CurrentValueKey(const UI_Document& document, int32 trackIndex, bool selectKey)
    {
        UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr == target || trackIndex < 0 || trackIndex >= static_cast<int32>(target->tracks.size()))
            return false;

        UI_AnimationKeyframeTrack& track = target->tracks[trackIndex];
        if (track.type == UI_AnimationKeyframeTrackType::Color)
            return Upsert_KeyColor(_currentFrame, Get_TargetCurrentColor255(document), track.easing, selectKey);

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

    bool UI_AnimationAssetService::Upsert_KeyValue(UI_AnimationKeyframeTrackType trackType, int32 frame, float value, UI_AnimationEasing easing, bool selectKey)
    {
        if (trackType == UI_AnimationKeyframeTrackType::Alpha)
        {
            const ImVec4 color = Make_Color255WithAlpha01(ImVec4(255.f, 255.f, 255.f, 255.f), value);
            return Upsert_KeyColor(frame, color, easing, selectKey);
        }

        const int32 trackIndex = Ensure_Track(trackType);
        UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr == target || trackIndex < 0 || trackIndex >= static_cast<int32>(target->tracks.size()))
            return false;

        UI_AnimationKeyframeTrack& track = target->tracks[trackIndex];
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
        UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr == target || trackIndex < 0 || trackIndex >= static_cast<int32>(target->tracks.size()))
            return false;

        UI_AnimationKeyframeTrack& track = target->tracks[trackIndex];
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
            else
                normalizedKey.value = Clamp_AnimationKeyframeTrackValue(track.type, normalizedKey.value);

            if (!mergedKeys.empty() && mergedKeys.back().frame == normalizedKey.frame)
            {
                mergedKeys.back() = normalizedKey;
                continue;
            }

            mergedKeys.push_back(normalizedKey);
        }

        track.keys = move(mergedKeys);
        if (_selectedKeyIndex >= static_cast<int32>(track.keys.size()))
            _selectedKeyIndex = static_cast<int32>(track.keys.size()) - 1;
    }

    void UI_AnimationAssetService::Clamp_SelectedIndices()
    {
        if (_asset.targets.empty())
        {
            _selectedTargetIndex = -1;
            _selectedTrackIndex = -1;
            _selectedKeyIndex = -1;
            return;
        }

        if (_selectedTargetIndex < 0 || _selectedTargetIndex >= static_cast<int32>(_asset.targets.size()))
            _selectedTargetIndex = 0;

        UI_AnimationTargetBinding& target = _asset.targets[_selectedTargetIndex];
        if (target.tracks.empty())
        {
            _selectedTrackIndex = -1;
            _selectedKeyIndex = -1;
            return;
        }

        if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(target.tracks.size()))
            _selectedTrackIndex = 0;

        UI_AnimationKeyframeTrack& track = target.tracks[_selectedTrackIndex];
        if (track.keys.empty())
        {
            _selectedKeyIndex = -1;
            return;
        }

        if (_selectedKeyIndex >= static_cast<int32>(track.keys.size()))
            _selectedKeyIndex = static_cast<int32>(track.keys.size()) - 1;
    }
}
