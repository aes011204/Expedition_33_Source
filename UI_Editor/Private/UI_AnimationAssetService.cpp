#include "pch.h"
#include "UI_AnimationAssetService.h"

namespace UIEditor
{
    UI_AnimationAssetService::UI_AnimationAssetService()
    {
        New_Asset();
    }

    bool UI_AnimationAssetService::Is_PublicAnimationMode() const
    {
        return Is_PublicAnimation(_asset);
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
        Reset_EditHistory();
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
        Reset_EditHistory();
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

    bool UI_AnimationAssetService::Undo_AssetEdit()
    {
        UI_AnimationAssetHistoryState undoState = {};
        if (!_editHistory.Undo(
            Capture_HistoryState(),
            undoState,
            [this](const UI_AnimationAssetHistoryState& lhs, const UI_AnimationAssetHistoryState& rhs)
            {
                return Is_SameHistoryAsset(lhs, rhs);
            }))
        {
            return false;
        }

        Restore_HistoryState(undoState);
        _assetDirty = true;
        return true;
    }

    bool UI_AnimationAssetService::Redo_AssetEdit()
    {
        UI_AnimationAssetHistoryState redoState = {};
        if (!_editHistory.Redo(
            Capture_HistoryState(),
            redoState,
            [this](const UI_AnimationAssetHistoryState& lhs, const UI_AnimationAssetHistoryState& rhs)
            {
                return Is_SameHistoryAsset(lhs, rhs);
            }))
        {
            return false;
        }

        Restore_HistoryState(redoState);
        _assetDirty = true;
        return true;
    }

    bool UI_AnimationAssetService::Bind_SelectedUI(const UI_Object* selectedObject)
    {
        if (nullptr == selectedObject)
            return false;

        if (Is_PublicAnimationMode())
        {
            UI_AnimationTargetBinding previewTarget = {};
            previewTarget.targetName = selectedObject->name;
            previewTarget.targetObjectId = selectedObject->id;
            _asset.targets.clear();
            _asset.targets.push_back(move(previewTarget));
            _targetBindingDirty = false;
            _selectedTargetIndex = 0;
            _selectedKeyIndex = -1;
            Clamp_SelectedIndices();
            return true;
        }

        int32 targetIndex = Find_TargetIndexByObjectId(selectedObject->id);
        if (targetIndex < 0)
            targetIndex = Find_TargetIndexByName(selectedObject->name);

        if (targetIndex >= 0)
        {
            _selectedTargetIndex = targetIndex;
            const bool renamedTarget = _asset.targets[targetIndex].targetName != selectedObject->name;
            bool copiedPublicTracks = false;
            _asset.targets[targetIndex].targetName = selectedObject->name;
            _asset.targets[targetIndex].targetObjectId = selectedObject->id;
            if (_asset.targets[targetIndex].tracks.empty() && !_asset.tracks.empty())
            {
                _asset.targets[targetIndex].tracks = _asset.tracks;
                copiedPublicTracks = true;
            }
            _targetBindingDirty = false;
            Clamp_SelectedIndices();
            if (renamedTarget || copiedPublicTracks)
                Mark_AssetDirty();
            return true;
        }

        UI_AnimationTargetBinding target = {};
        target.targetName = selectedObject->name;
        target.targetObjectId = selectedObject->id;
        if (!_asset.tracks.empty())
            target.tracks = _asset.tracks;
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
        if (Is_PublicAnimationMode())
            return;
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
        if (Is_PublicAnimationMode())
        {
            if (_asset.targets.empty())
            {
                UI_AnimationTargetBinding previewTarget = {};
                previewTarget.targetName = targetName;
                _asset.targets.push_back(move(previewTarget));
                _selectedTargetIndex = 0;
            }
            else
            {
                _asset.targets[0].targetName = targetName;
                _selectedTargetIndex = 0;
            }

            _targetBindingDirty = true;
            Resolve_TargetObject(document, true);
            return;
        }

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

    void UI_AnimationAssetService::Set_AnimationType(UI_AnimationAssetType animationType)
    {
        if (_asset.animationType == animationType)
            return;

        if (animationType == UI_AnimationAssetType::Public_Animation && _asset.tracks.empty())
        {
            const UI_AnimationTargetBinding* target = Get_SelectedTarget();
            if (nullptr != target && !target->tracks.empty())
                _asset.tracks = target->tracks;

            if (nullptr != target)
            {
                UI_AnimationTargetBinding previewTarget = *target;
                previewTarget.tracks.clear();
                _asset.targets.clear();
                _asset.targets.push_back(move(previewTarget));
                _selectedTargetIndex = 0;
            }
        }
        else if (animationType == UI_AnimationAssetType::Target_Animation && !_asset.tracks.empty())
        {
            UI_AnimationTargetBinding* target = Get_SelectedTarget();
            if (nullptr != target && target->tracks.empty())
                target->tracks = _asset.tracks;
        }

        _asset.animationType = animationType;
        _selectedTrackIndex = -1;
        _selectedKeyIndex = -1;
        Clamp_SelectedIndices();
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

        for (UI_AnimationKeyframeTrack& track : _asset.tracks)
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
        }

        for (UI_AnimationTargetBinding& target : _asset.targets)
        {
            for (UI_AnimationKeyframeTrack& track : target.tracks)
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
        for (UI_AnimationKeyframeTrack& track : _asset.tracks)
            Sort_TrackKeys(track);

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
        if (Is_PublicAnimationMode())
        {
            if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
                return nullptr;

            return &_asset.tracks[_selectedTrackIndex];
        }

        UI_AnimationTargetBinding* target = Get_SelectedTarget();
        if (nullptr == target || _selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(target->tracks.size()))
            return nullptr;

        return &target->tracks[_selectedTrackIndex];
    }

    const UI_AnimationKeyframeTrack* UI_AnimationAssetService::Get_SelectedTrack() const
    {
        if (Is_PublicAnimationMode())
        {
            if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
                return nullptr;

            return &_asset.tracks[_selectedTrackIndex];
        }

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
        if (Is_PublicAnimationMode())
            return Get_AnimationKeyframeTrackDefaultValue(trackType);

        const UI_AnimationTargetBinding* target = Get_SelectedTarget();
        const UI_Object* targetObject = nullptr != target ? document.Find_Object(target->targetObjectId) : nullptr;
        if (nullptr == targetObject)
            return Get_AnimationKeyframeTrackDefaultValue(trackType);

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
        case UI_AnimationKeyframeTrackType::AlphaMultiplier:
            return targetObject->alphaMultiplier;
        case UI_AnimationKeyframeTrackType::FontSize:
            return targetObject->fontSize;
        case UI_AnimationKeyframeTrackType::FillRatio:
            return targetObject->fillRatio;
        case UI_AnimationKeyframeTrackType::Color:
            return 0.f;
        default:
            return 0.f;
        }
    }

    ImVec2 UI_AnimationAssetService::Get_TargetCurrentVec2Value(const UI_Document& document, UI_AnimationKeyframeTrackType trackType) const
    {
        if (trackType == UI_AnimationKeyframeTrackType::Size)
        {
            if (Is_PublicAnimationMode())
                return ImVec2(100.f, 100.f);

            const UI_AnimationTargetBinding* target = Get_SelectedTarget();
            const UI_Object* targetObject = nullptr != target ? document.Find_Object(target->targetObjectId) : nullptr;
            if (nullptr == targetObject)
                return ImVec2(100.f, 100.f);

            return targetObject->size;
        }

        if (trackType == UI_AnimationKeyframeTrackType::PatternUvOffset)
        {
            if (Is_PublicAnimationMode())
                return Get_DefaultPatternUvOffset();

            const UI_AnimationTargetBinding* target = Get_SelectedTarget();
            const UI_Object* targetObject = nullptr != target ? document.Find_Object(target->targetObjectId) : nullptr;
            if (nullptr == targetObject)
                return Get_DefaultPatternUvOffset();

            return targetObject->patternUvOffset;
        }

        if (trackType == UI_AnimationKeyframeTrackType::PatternUvScale)
        {
            if (Is_PublicAnimationMode())
                return Get_DefaultPatternUvScale();

            const UI_AnimationTargetBinding* target = Get_SelectedTarget();
            const UI_Object* targetObject = nullptr != target ? document.Find_Object(target->targetObjectId) : nullptr;
            if (nullptr == targetObject)
                return Get_DefaultPatternUvScale();

            return targetObject->patternUvScale;
        }

        return ImVec2(0.f, 0.f);
    }

    ImVec4 UI_AnimationAssetService::Get_TargetCurrentColor255(const UI_Document& document) const
    {
        const UI_AnimationTargetBinding* target = Get_SelectedTarget();
        const UI_Object* targetObject = nullptr != target ? document.Find_Object(target->targetObjectId) : nullptr;
        if (nullptr == targetObject)
            return ImVec4(255.f, 255.f, 255.f, 255.f);

        return Convert_AnimationColor01To255(targetObject->color);
    }

    bool UI_AnimationAssetService::Remove_SelectedTarget()
    {
        if (_selectedTargetIndex < 0 || _selectedTargetIndex >= static_cast<int32>(_asset.targets.size()))
            return false;

        _asset.targets.erase(_asset.targets.begin() + _selectedTargetIndex);
        if (_selectedTargetIndex >= static_cast<int32>(_asset.targets.size()))
            _selectedTargetIndex = static_cast<int32>(_asset.targets.size()) - 1;

        if (Is_PublicAnimationMode())
        {
            _targetBindingDirty = false;
            return true;
        }

        if (!Is_PublicAnimationMode())
        {
            _selectedTrackIndex = -1;
            _selectedKeyIndex = -1;
            if (nullptr != Get_SelectedTarget() && !Get_SelectedTarget()->tracks.empty())
                _selectedTrackIndex = 0;
        }

        Mark_AssetDirty();
        return true;
    }

    void UI_AnimationAssetService::Mark_AssetDirty()
    {
        _editHistory.Capture_Edit(
            Capture_HistoryState(),
            [this](const UI_AnimationAssetHistoryState& lhs, const UI_AnimationAssetHistoryState& rhs)
            {
                return Is_SameHistoryAsset(lhs, rhs);
            });

        _assetDirty = true;
    }

    UI_AnimationAssetService::UI_AnimationAssetHistoryState UI_AnimationAssetService::Capture_HistoryState() const
    {
        UI_AnimationAssetHistoryState historyState = {};
        historyState.asset = _asset;
        historyState.currentFrame = _currentFrame;
        historyState.selectedTargetIndex = _selectedTargetIndex;
        historyState.selectedTrackIndex = _selectedTrackIndex;
        historyState.selectedKeyIndex = _selectedKeyIndex;
        historyState.pendingTrackType = _pendingTrackType;
        return historyState;
    }

    void UI_AnimationAssetService::Restore_HistoryState(const UI_AnimationAssetHistoryState& historyState)
    {
        _asset = historyState.asset;
        _currentFrame = historyState.currentFrame;
        _selectedTargetIndex = historyState.selectedTargetIndex;
        _selectedTrackIndex = historyState.selectedTrackIndex;
        _selectedKeyIndex = historyState.selectedKeyIndex;
        _pendingTrackType = historyState.pendingTrackType;
        _targetBindingDirty = true;
        Clamp_Asset();
        Clamp_SelectedIndices();
    }

    bool UI_AnimationAssetService::Is_SameHistoryAsset(
        const UI_AnimationAssetHistoryState& lhs,
        const UI_AnimationAssetHistoryState& rhs) const
    {
        return UI_AnimationAssetSerializer::To_Json(lhs.asset) == UI_AnimationAssetSerializer::To_Json(rhs.asset);
    }

    void UI_AnimationAssetService::Reset_EditHistory()
    {
        _editHistory.Reset(Capture_HistoryState());
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

    void UI_AnimationAssetService::Clamp_SelectedIndices()
    {
        if (_asset.targets.empty())
        {
            _selectedTargetIndex = -1;
            if (!Is_PublicAnimationMode())
            {
                _selectedTrackIndex = -1;
                _selectedKeyIndex = -1;
                return;
            }
        }
        else if (_selectedTargetIndex < 0 || _selectedTargetIndex >= static_cast<int32>(_asset.targets.size()))
        {
            _selectedTargetIndex = 0;
        }

        if (Is_PublicAnimationMode())
        {
            if (_asset.tracks.empty())
            {
                _selectedTrackIndex = -1;
                _selectedKeyIndex = -1;
                return;
            }

            if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
                _selectedTrackIndex = 0;

            UI_AnimationKeyframeTrack& track = _asset.tracks[_selectedTrackIndex];
            if (track.keys.empty())
            {
                _selectedKeyIndex = -1;
                return;
            }

            if (_selectedKeyIndex >= static_cast<int32>(track.keys.size()))
                _selectedKeyIndex = static_cast<int32>(track.keys.size()) - 1;
            return;
        }

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
