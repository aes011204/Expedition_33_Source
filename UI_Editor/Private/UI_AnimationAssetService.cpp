#include "pch.h"
#include "UI_AnimationAssetService.h"

#include <commdlg.h>

namespace UIEditor
{
    namespace
    {
        constexpr wchar_t kDefaultAnimationFileName[] = L"NewUIAnimation.uianim.json";

        fs::path Get_ModuleDirectory()
        {
            wchar_t modulePath[MAX_PATH] = {};
            if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
                return {};

            return fs::path(modulePath).parent_path();
        }

        bool Try_CreateDirectory(const fs::path& directory)
        {
            if (directory.empty())
                return false;

            try
            {
                if (fs::exists(directory))
                    return fs::is_directory(directory);

                return fs::create_directories(directory);
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI animation directory create failed: {} ({})", exception.what(), String::ToString(directory.wstring()));
                return false;
            }
        }

        bool Try_FindResourceRootFromStart(const fs::path& startPath, fs::path& outResourceRoot)
        {
            if (startPath.empty())
                return false;

            try
            {
                fs::path start = fs::absolute(startPath).lexically_normal();
                for (fs::path currentPath = start; !currentPath.empty(); currentPath = currentPath.parent_path())
                {
                    const fs::path candidate = (currentPath / L"Client" / L"Bin" / L"Resources").lexically_normal();
                    if (fs::exists(candidate) && fs::is_directory(candidate))
                    {
                        outResourceRoot = candidate;
                        return true;
                    }

                    if (currentPath == currentPath.parent_path())
                        break;
                }
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI animation resource root search failed: {}", exception.what());
            }

            return false;
        }

        bool Ends_With(const wstring& value, const wstring& suffix)
        {
            if (value.size() < suffix.size())
                return false;

            return equal(suffix.rbegin(), suffix.rend(), value.rbegin());
        }

        wstring To_Lower(wstring value)
        {
            transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](wchar_t ch)
                {
                    return static_cast<wchar_t>(towlower(ch));
                });
            return value;
        }
    }

    UI_AnimationAssetService::UI_AnimationAssetService()
    {
        New_Asset();
    }

    void UI_AnimationAssetService::New_Asset()
    {
        _asset = {};
        _assetFilePath.clear();
        _assetDirty = false;
        _previewEnabled = true;
        _currentFrame = _asset.startFrame;
        _selectedTrackIndex = -1;
        _selectedKeyIndex = -1;
        _pendingTrackType = UI_AnimationKeyframeTrackType::PositionX;
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
        _selectedTrackIndex = _asset.tracks.empty() ? -1 : 0;
        _selectedKeyIndex = -1;
        Clamp_Asset();
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

        _asset.targetName = selectedObject->name;
        _asset.targetObjectId = selectedObject->id;
        Mark_AssetDirty();
        return true;
    }

    void UI_AnimationAssetService::Clear_Target()
    {
        _asset.targetName.clear();
        _asset.targetObjectId = 0;
        Mark_AssetDirty();
    }

    void UI_AnimationAssetService::Resolve_TargetObject(const UI_Document& document)
    {
        if (_asset.targetName.empty())
        {
            _asset.targetObjectId = 0;
            return;
        }

        const UI_Object* currentTarget = document.Find_Object(_asset.targetObjectId);
        if (nullptr != currentTarget && currentTarget->name == _asset.targetName)
            return;

        _asset.targetObjectId = 0;
        for (const UI_Object& object : document.Get_Objects())
        {
            if (object.name == _asset.targetName)
            {
                _asset.targetObjectId = object.id;
                return;
            }
        }
    }

    void UI_AnimationAssetService::Set_TargetName(const string& targetName, const UI_Document& document)
    {
        _asset.targetName = targetName;
        Resolve_TargetObject(document);
        Mark_AssetDirty();
    }

    void UI_AnimationAssetService::Set_CurrentFrame(int32 frame)
    {
        Clamp_Asset();
        _currentFrame = min(max(frame, _asset.startFrame), _asset.endFrame);
    }

    void UI_AnimationAssetService::Clamp_Asset()
    {
        _asset.fps = max(1, _asset.fps);
        _asset.endFrame = max(_asset.startFrame + 1, _asset.endFrame);
        _currentFrame = min(max(_currentFrame, _asset.startFrame), _asset.endFrame);

        for (UI_AnimationKeyframeTrack& track : _asset.tracks)
        {
            for (UI_AnimationKeyframe& key : track.keys)
            {
                key.frame = min(max(key.frame, _asset.startFrame), _asset.endFrame);
                key.value = Clamp_AnimationKeyframeTrackValue(track.type, key.value);
            }
        }
    }

    void UI_AnimationAssetService::Normalize_AllTracks()
    {
        for (UI_AnimationKeyframeTrack& track : _asset.tracks)
            Sort_TrackKeys(track);
    }

    UI_AnimationKeyframeTrack* UI_AnimationAssetService::Get_SelectedTrack()
    {
        if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
            return nullptr;

        return &_asset.tracks[_selectedTrackIndex];
    }

    const UI_AnimationKeyframeTrack* UI_AnimationAssetService::Get_SelectedTrack() const
    {
        if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
            return nullptr;

        return &_asset.tracks[_selectedTrackIndex];
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
        const UI_Object* targetObject = document.Find_Object(_asset.targetObjectId);
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
        default:
            return 0.f;
        }
    }

    bool UI_AnimationAssetService::Add_Track(UI_AnimationKeyframeTrackType trackType)
    {
        for (int32 index = 0; index < static_cast<int32>(_asset.tracks.size()); ++index)
        {
            if (_asset.tracks[index].type == trackType)
            {
                _selectedTrackIndex = index;
                _selectedKeyIndex = -1;
                return false;
            }
        }

        UI_AnimationKeyframeTrack track = {};
        track.type = trackType;
        track.enabled = true;
        _asset.tracks.push_back(track);
        _selectedTrackIndex = static_cast<int32>(_asset.tracks.size()) - 1;
        _selectedKeyIndex = -1;
        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Delete_SelectedTrack()
    {
        if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
            return false;

        _asset.tracks.erase(_asset.tracks.begin() + _selectedTrackIndex);
        if (_selectedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
            _selectedTrackIndex = static_cast<int32>(_asset.tracks.size()) - 1;
        _selectedKeyIndex = -1;
        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Add_KeyToSelectedTrack(const UI_Document& document)
    {
        UI_AnimationKeyframeTrack* track = Get_SelectedTrack();
        if (nullptr == track)
            return false;

        const float value = Clamp_AnimationKeyframeTrackValue(track->type, Get_TargetCurrentValue(document, track->type));
        for (int32 index = 0; index < static_cast<int32>(track->keys.size()); ++index)
        {
            if (track->keys[index].frame == _currentFrame)
            {
                track->keys[index].value = value;
                _selectedKeyIndex = index;
                Mark_AssetDirty();
                return true;
            }
        }

        UI_AnimationKeyframe key = {};
        key.frame = _currentFrame;
        key.value = value;
        track->keys.push_back(key);
        Sort_TrackKeys(*track);

        for (int32 index = 0; index < static_cast<int32>(track->keys.size()); ++index)
        {
            if (track->keys[index].frame == _currentFrame)
            {
                _selectedKeyIndex = index;
                break;
            }
        }

        Mark_AssetDirty();
        return true;
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

    void UI_AnimationAssetService::Select_Track(int32 trackIndex)
    {
        if (trackIndex < 0 || trackIndex >= static_cast<int32>(_asset.tracks.size()))
            return;

        _selectedTrackIndex = trackIndex;
        _selectedKeyIndex = -1;
    }

    bool UI_AnimationAssetService::Select_Key(int32 trackIndex, int32 keyIndex)
    {
        if (trackIndex < 0 || trackIndex >= static_cast<int32>(_asset.tracks.size()))
            return false;

        if (keyIndex < 0 || keyIndex >= static_cast<int32>(_asset.tracks[trackIndex].keys.size()))
            return false;

        _selectedTrackIndex = trackIndex;
        _selectedKeyIndex = keyIndex;
        return true;
    }

    bool UI_AnimationAssetService::Set_SelectedKeyFrame(int32 frame, int32& outFrame)
    {
        UI_AnimationKeyframeTrack* editableTrack = Get_SelectedTrack();
        UI_AnimationKeyframe* selectedKey = Get_SelectedKey();
        if (nullptr == editableTrack || nullptr == selectedKey)
            return false;

        selectedKey->frame = min(max(frame, _asset.startFrame), _asset.endFrame);
        selectedKey->value = Clamp_AnimationKeyframeTrackValue(editableTrack->type, selectedKey->value);
        const int32 targetFrame = selectedKey->frame;
        Sort_TrackKeys(*editableTrack);

        for (int32 index = 0; index < static_cast<int32>(editableTrack->keys.size()); ++index)
        {
            if (editableTrack->keys[index].frame == targetFrame)
            {
                _selectedKeyIndex = index;
                break;
            }
        }

        outFrame = targetFrame;
        Mark_AssetDirty();
        return true;
    }

    bool UI_AnimationAssetService::Set_SelectedKeyValue(float value)
    {
        UI_AnimationKeyframeTrack* editableTrack = Get_SelectedTrack();
        UI_AnimationKeyframe* selectedKey = Get_SelectedKey();
        if (nullptr == editableTrack || nullptr == selectedKey)
            return false;

        selectedKey->value = Clamp_AnimationKeyframeTrackValue(editableTrack->type, value);
        Mark_AssetDirty();
        return true;
    }

    void UI_AnimationAssetService::Mark_AssetDirty()
    {
        _assetDirty = true;
    }

    bool UI_AnimationAssetService::Show_SaveAssetDialog(wstring& outFilePath) const
    {
        wchar_t filePathBuffer[MAX_PATH] = {};
        if (!outFilePath.empty())
            wcsncpy_s(filePathBuffer, outFilePath.c_str(), _TRUNCATE);
        else
            wcsncpy_s(filePathBuffer, kDefaultAnimationFileName, _TRUNCATE);

        const wstring initialDirectory = Resolve_DefaultAnimationDirectory();

        OPENFILENAMEW openFileName = {};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.lpstrFilter = L"UI Animation Files\0*.uianim.json\0JSON Files\0*.json\0All Files\0*.*\0";
        openFileName.lpstrFile = filePathBuffer;
        openFileName.nMaxFile = MAX_PATH;
        if (!initialDirectory.empty())
            openFileName.lpstrInitialDir = initialDirectory.c_str();
        openFileName.lpstrDefExt = L"uianim.json";
        openFileName.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (FALSE == GetSaveFileNameW(&openFileName))
            return false;

        outFilePath = filePathBuffer;
        Ensure_AnimationExtension(outFilePath);
        return !outFilePath.empty();
    }

    bool UI_AnimationAssetService::Show_LoadAssetDialog(wstring& outFilePath) const
    {
        wchar_t filePathBuffer[MAX_PATH] = {};
        const wstring initialDirectory = Resolve_DefaultAnimationDirectory();

        OPENFILENAMEW openFileName = {};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.lpstrFilter = L"UI Animation Files\0*.uianim.json\0JSON Files\0*.json\0All Files\0*.*\0";
        openFileName.lpstrFile = filePathBuffer;
        openFileName.nMaxFile = MAX_PATH;
        if (!initialDirectory.empty())
            openFileName.lpstrInitialDir = initialDirectory.c_str();
        openFileName.lpstrDefExt = L"uianim.json";
        openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (FALSE == GetOpenFileNameW(&openFileName))
            return false;

        outFilePath = filePathBuffer;
        return !outFilePath.empty();
    }

    wstring UI_AnimationAssetService::Resolve_DefaultAnimationDirectory() const
    {
        vector<fs::path> startPaths;

        const fs::path moduleDirectory = Get_ModuleDirectory();
        if (!moduleDirectory.empty())
            startPaths.push_back(moduleDirectory);

        try
        {
            startPaths.push_back(fs::current_path());
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI animation current path query failed: {}", exception.what());
        }

        for (const fs::path& startPath : startPaths)
        {
            fs::path resourceRoot = {};
            if (Try_FindResourceRootFromStart(startPath, resourceRoot))
            {
                const fs::path animationDirectory = (resourceRoot / L"UI" / L"UI_Animations").lexically_normal();
                if (Try_CreateDirectory(animationDirectory))
                    return animationDirectory.wstring();
            }
        }

        const vector<fs::path> fallbackCandidates = {
            L"..\\Bin\\Resources\\UI\\UI_Animations",
            L"..\\Client\\Bin\\Resources\\UI\\UI_Animations",
            L"..\\..\\Client\\Bin\\Resources\\UI\\UI_Animations",
            L"..\\..\\..\\Client\\Bin\\Resources\\UI\\UI_Animations",
            L"Client\\Bin\\Resources\\UI\\UI_Animations",
        };

        for (const fs::path& candidate : fallbackCandidates)
        {
            try
            {
                const fs::path normalizedCandidate = fs::absolute(candidate).lexically_normal();
                if (Try_CreateDirectory(normalizedCandidate))
                    return normalizedCandidate.wstring();
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI animation fallback directory failed: {} ({})", exception.what(), String::ToString(candidate.wstring()));
            }
        }

        return {};
    }

    void UI_AnimationAssetService::Ensure_AnimationExtension(wstring& filePath) const
    {
        if (filePath.empty())
            return;

        fs::path path(filePath);
        const wstring lowerFileName = To_Lower(path.filename().wstring());
        if (Ends_With(lowerFileName, L".uianim.json"))
            return;

        path.replace_extension(L".uianim.json");
        filePath = path.wstring();
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
}
