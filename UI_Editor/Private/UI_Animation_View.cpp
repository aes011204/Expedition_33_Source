#include "pch.h"
#include "UI_Animation_View.h"

#include "UI_Editor_Manager.h"

#include <commdlg.h>

namespace UIEditor
{
    namespace
    {
        constexpr wchar_t kDefaultAnimationFileName[] = L"NewUIAnimation.uianim.json";
        constexpr float kTrackPanelWidth = 255.f;
        constexpr float kKeyPanelWidth = 220.f;
        constexpr float kRulerHeight = 30.f;
        constexpr float kTrackRowHeight = 34.f;
        constexpr float kFrameWidth = 18.f;
        constexpr float kPi = 3.14159265358979323846f;

        const char* const kTrackTypeNames[] = {
            "PositionX",
            "PositionY",
            "ScaleX",
            "ScaleY",
            "Rotation",
            "Alpha",
        };

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

        int32 Get_TrackTypeIndex(UIAnimationTrackType type)
        {
            return min(max(static_cast<int32>(type), 0), static_cast<int32>(size(kTrackTypeNames)) - 1);
        }

        float Get_ValueDragSpeed(UIAnimationTrackType type)
        {
            switch (type)
            {
            case UIAnimationTrackType::PositionX:
            case UIAnimationTrackType::PositionY:
            case UIAnimationTrackType::Rotation:
                return 1.f;
            case UIAnimationTrackType::Alpha:
            case UIAnimationTrackType::ScaleX:
            case UIAnimationTrackType::ScaleY:
            default:
                return 0.01f;
            }
        }

        float Radians_ToDegrees(float radians)
        {
            return radians * 180.f / kPi;
        }

        float Degrees_ToRadians(float degrees)
        {
            return degrees * kPi / 180.f;
        }

        string Make_KeyValueLabel(const UIAnimationTrack& track, const UIAnimationKeyframe& key)
        {
            if (track.type == UIAnimationTrackType::Rotation)
                return to_string(Radians_ToDegrees(key.value)) + " deg";

            return to_string(key.value);
        }
    }

    UI_Animation_View::UI_Animation_View(UI_Editor_Manager& editorManager)
        : UI_Editor_Window{ L"Animation" }
        , _editorManager{ editorManager }
    {
        New_Asset();
    }

    void UI_Animation_View::Render()
    {
        if (!Is_Open())
            return;

        bool isOpen = Is_Open();
        const string windowName = String::ToString(Get_WindowName());

        if (ImGui::Begin(windowName.c_str(), &isOpen))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

            Clamp_Asset();
            Resolve_TargetObject();

            if (_editorManager.Is_AnimationPreviewActive() && _asset.targetObjectId != 0)
                _currentFrame = _editorManager.Get_AnimationPreviewFrame();

            Render_Toolbar();
            ImGui::Separator();
            Render_TargetSection();
            ImGui::Separator();
            Render_PlaybackSection();
            ImGui::Separator();
            Render_TimelineSection();

            if (!_previewEnabled && _editorManager.Is_AnimationPreviewActive())
                _editorManager.Stop_AnimationPreview();
            else if (_previewEnabled && !_editorManager.Is_AnimationPreviewPlaying())
                Refresh_StaticPreview();
        }

        Set_Open(isOpen);
        ImGui::End();
    }

    void UI_Animation_View::Render_Toolbar()
    {
        if (ImGui::Button("New") && Confirm_DiscardDirtyAsset(L"create a new animation asset"))
            New_Asset();
        ImGui::SameLine();
        if (ImGui::Button("Load") && Confirm_DiscardDirtyAsset(L"load another animation asset"))
            Load_Asset();
        ImGui::SameLine();
        if (ImGui::Button("Save"))
            Save_Asset(false);
        ImGui::SameLine();
        if (ImGui::Button("Save As"))
            Save_Asset(true);

        ImGui::SameLine();
        ImGui::TextDisabled(
            "Asset: %s%s",
            _asset.name.empty() ? "NewUIAnimation" : _asset.name.c_str(),
            _assetDirty ? " *" : "");
    }

    void UI_Animation_View::Render_TargetSection()
    {
        if (ImGui::Button("Bind Selected UI"))
            Bind_SelectedUI();
        ImGui::SameLine();
        if (ImGui::Button("Clear Target"))
            Clear_Target();

        if (ImGui::InputText("Target Name", _targetNameBuffer, size(_targetNameBuffer)))
        {
            _asset.targetName = _targetNameBuffer;
            Resolve_TargetObject();
            Mark_AssetDirty();
            Refresh_StaticPreview();
        }

        const char* boundName = "None";
        if (_asset.targetObjectId != 0)
        {
            const UI_Object* targetObject = _editorManager.Get_Document().Find_Object(_asset.targetObjectId);
            if (nullptr != targetObject)
                boundName = targetObject->name.c_str();
        }

        ImGui::Text("Bound UI: %s", boundName);
    }

    void UI_Animation_View::Render_PlaybackSection()
    {
        const bool hasTarget = _asset.targetObjectId != 0;
        ImGui::BeginDisabled(!hasTarget);
        if (ImGui::Button("Play"))
            _editorManager.Play_AnimationAssetPreview(_asset, _currentFrame);
        ImGui::SameLine();
        if (ImGui::Button("Pause"))
        {
            if (_editorManager.Is_AnimationPreviewActive())
                _currentFrame = _editorManager.Get_AnimationPreviewFrame();
            _editorManager.Pause_AnimationPreview();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            _editorManager.Stop_AnimationPreview();
            Set_CurrentFrame(_asset.startFrame);
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Checkbox("Enable Preview", &_previewEnabled))
        {
            if (!_previewEnabled)
                _editorManager.Stop_AnimationPreview();
            else
                Refresh_StaticPreview();
        }

        bool changed = false;
        ImGui::SetNextItemWidth(78.f);
        changed |= ImGui::InputInt("FPS", &_asset.fps);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(78.f);
        changed |= ImGui::InputInt("Start", &_asset.startFrame);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(78.f);
        changed |= ImGui::InputInt("End", &_asset.endFrame);
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Loop", &_asset.loop);

        int32 editableCurrentFrame = _currentFrame;
        ImGui::SetNextItemWidth(78.f);
        if (ImGui::InputInt("Current Frame", &editableCurrentFrame))
            Set_CurrentFrame(editableCurrentFrame);

        if (changed)
        {
            Clamp_Asset();
            for (UIAnimationTrack& track : _asset.tracks)
                Sort_TrackKeys(track);
            Set_CurrentFrame(_currentFrame);
            Mark_AssetDirty();
        }
    }

    void UI_Animation_View::Render_TimelineSection()
    {
        if (ImGui::BeginTable("AnimationTimelineLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Tracks", ImGuiTableColumnFlags_WidthFixed, kTrackPanelWidth);
            ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, kKeyPanelWidth);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            Render_TrackPanel();

            ImGui::TableSetColumnIndex(1);
            const ImVec2 timelineSize = ImVec2(ImGui::GetContentRegionAvail().x, max(260.f, ImGui::GetContentRegionAvail().y));
            Render_TimelineCanvas(timelineSize);

            ImGui::TableSetColumnIndex(2);
            Render_KeyPanel();

            ImGui::EndTable();
        }
    }

    void UI_Animation_View::Render_TrackPanel()
    {
        ImGui::Text("Tracks: %zu", _asset.tracks.size());
        ImGui::Separator();

        const float trackListHeight = 130.f;
        if (ImGui::BeginChild("AnimationTrackList", ImVec2(0.f, trackListHeight), true))
        {
            for (int32 index = 0; index < static_cast<int32>(_asset.tracks.size()); ++index)
            {
                const UIAnimationTrack& track = _asset.tracks[index];
                const string label = string(Get_UIAnimationTrackTypeName(track.type)) + " (" + to_string(track.keys.size()) + ")";
                if (ImGui::Selectable(label.c_str(), _selectedTrackIndex == index))
                {
                    _selectedTrackIndex = index;
                    _selectedKeyIndex = -1;
                }
            }
        }
        ImGui::EndChild();

        int32 trackTypeIndex = Get_TrackTypeIndex(_pendingTrackType);
        if (ImGui::Combo("Add Track Type", &trackTypeIndex, kTrackTypeNames, static_cast<int32>(size(kTrackTypeNames))))
            _pendingTrackType = static_cast<UIAnimationTrackType>(trackTypeIndex);

        if (ImGui::Button("Add Track"))
            Add_Track(_pendingTrackType);

        ImGui::BeginDisabled(nullptr == Get_SelectedTrack());
        if (ImGui::Button("Delete Track"))
            Delete_SelectedTrack();
        if (ImGui::Button("Add Key To Selected Track"))
            Add_KeyToSelectedTrack();
        ImGui::EndDisabled();

        ImGui::BeginDisabled(nullptr == Get_SelectedKey());
        if (ImGui::Button("Delete Key"))
            Delete_SelectedKey();
        ImGui::EndDisabled();
    }

    void UI_Animation_View::Render_TimelineCanvas(const ImVec2& size)
    {
        Clamp_Asset();

        const int32 frameCount = max(1, _asset.endFrame - _asset.startFrame + 1);
        const float contentWidth = max(size.x, static_cast<float>(frameCount) * kFrameWidth + 32.f);
        const float contentHeight = max(size.y, kRulerHeight + max(1, static_cast<int32>(_asset.tracks.size())) * kTrackRowHeight + 16.f);

        if (!ImGui::BeginChild("AnimationTimelineCanvas", size, true, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::EndChild();
            return;
        }

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("AnimationTimelineHitArea", ImVec2(contentWidth, contentHeight));
        const bool isCanvasHovered = ImGui::IsItemHovered();
        const bool isCanvasClicked = isCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const ImVec2 mousePos = ImGui::GetIO().MousePos;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 canvasMax = ImVec2(origin.x + contentWidth, origin.y + contentHeight);
        drawList->AddRectFilled(origin, canvasMax, IM_COL32(31, 31, 34, 255));

        auto frameToX = [&](int32 frame) -> float
        {
            return origin.x + static_cast<float>(frame - _asset.startFrame) * kFrameWidth;
        };

        auto xToFrame = [&](float x) -> int32
        {
            const int32 frame = _asset.startFrame + static_cast<int32>(roundf((x - origin.x) / kFrameWidth));
            return min(max(frame, _asset.startFrame), _asset.endFrame);
        };

        const int32 majorStep = frameCount > 240 ? 30 : (frameCount > 120 ? 20 : 10);
        for (int32 frame = _asset.startFrame; frame <= _asset.endFrame; ++frame)
        {
            const float x = frameToX(frame);
            const bool isMajor = ((frame - _asset.startFrame) % majorStep) == 0 || frame == _asset.endFrame;
            const ImU32 lineColor = isMajor ? IM_COL32(106, 110, 120, 180) : IM_COL32(70, 73, 80, 120);
            drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, canvasMax.y), lineColor, isMajor ? 1.2f : 1.f);

            if (isMajor)
            {
                const string label = to_string(frame);
                drawList->AddText(ImVec2(x + 3.f, origin.y + 5.f), IM_COL32(210, 210, 220, 230), label.c_str());
            }
        }

        drawList->AddLine(
            ImVec2(origin.x, origin.y + kRulerHeight),
            ImVec2(canvasMax.x, origin.y + kRulerHeight),
            IM_COL32(115, 118, 128, 180),
            1.5f);

        for (int32 trackIndex = 0; trackIndex < static_cast<int32>(_asset.tracks.size()); ++trackIndex)
        {
            const UIAnimationTrack& track = _asset.tracks[trackIndex];
            const float rowMinY = origin.y + kRulerHeight + static_cast<float>(trackIndex) * kTrackRowHeight;
            const float rowMaxY = rowMinY + kTrackRowHeight;
            const bool isSelectedTrack = _selectedTrackIndex == trackIndex;
            const ImU32 rowColor = isSelectedTrack ? IM_COL32(62, 66, 76, 255) : (trackIndex % 2 == 0 ? IM_COL32(42, 43, 48, 255) : IM_COL32(37, 38, 42, 255));
            drawList->AddRectFilled(ImVec2(origin.x, rowMinY), ImVec2(canvasMax.x, rowMaxY), rowColor);
            drawList->AddText(
                ImVec2(origin.x + 8.f, rowMinY + 9.f),
                IM_COL32(222, 224, 232, 235),
                Get_UIAnimationTrackTypeName(track.type));

            for (int32 keyIndex = 0; keyIndex < static_cast<int32>(track.keys.size()); ++keyIndex)
            {
                const UIAnimationKeyframe& key = track.keys[keyIndex];
                const float x = frameToX(key.frame);
                const float y = rowMinY + kTrackRowHeight * 0.5f;
                const bool isSelectedKey = _selectedTrackIndex == trackIndex && _selectedKeyIndex == keyIndex;
                const ImU32 keyColor = isSelectedKey ? IM_COL32(255, 230, 0, 255) : IM_COL32(255, 255, 255, 245);
                drawList->AddRectFilled(ImVec2(x - 5.f, y - 8.f), ImVec2(x + 5.f, y + 8.f), keyColor, 2.f);
                drawList->AddRect(ImVec2(x - 5.f, y - 8.f), ImVec2(x + 5.f, y + 8.f), IM_COL32(0, 0, 0, 220), 2.f);
            }
        }

        const float currentFrameX = frameToX(_currentFrame);
        drawList->AddLine(
            ImVec2(currentFrameX, origin.y),
            ImVec2(currentFrameX, canvasMax.y),
            IM_COL32(232, 56, 70, 255),
            2.5f);

        if (isCanvasClicked)
        {
            const int32 clickedFrame = xToFrame(mousePos.x);
            int32 clickedTrackIndex = -1;
            if (mousePos.y >= origin.y + kRulerHeight)
            {
                clickedTrackIndex = static_cast<int32>((mousePos.y - origin.y - kRulerHeight) / kTrackRowHeight);
                if (clickedTrackIndex < 0 || clickedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
                    clickedTrackIndex = -1;
            }

            bool hitKey = false;
            if (clickedTrackIndex >= 0)
            {
                _selectedTrackIndex = clickedTrackIndex;
                UIAnimationTrack& track = _asset.tracks[clickedTrackIndex];
                for (int32 keyIndex = 0; keyIndex < static_cast<int32>(track.keys.size()); ++keyIndex)
                {
                    const float keyX = frameToX(track.keys[keyIndex].frame);
                    if (fabsf(mousePos.x - keyX) <= 7.f)
                    {
                        Select_Key(clickedTrackIndex, keyIndex);
                        Set_CurrentFrame(track.keys[keyIndex].frame);
                        hitKey = true;
                        break;
                    }
                }

                if (!hitKey)
                    _selectedKeyIndex = -1;
            }

            if (!hitKey)
                Set_CurrentFrame(clickedFrame);
        }

        ImGui::EndChild();
    }

    void UI_Animation_View::Render_KeyPanel()
    {
        const UIAnimationTrack* selectedTrack = Get_SelectedTrack();
        if (nullptr == selectedTrack)
        {
            ImGui::TextDisabled("No track selected.");
            return;
        }

        ImGui::Text("Track: %s", Get_UIAnimationTrackTypeName(selectedTrack->type));
        ImGui::Text("Keys: %zu", selectedTrack->keys.size());
        ImGui::Separator();

        if (ImGui::BeginChild("AnimationKeyList", ImVec2(0.f, 120.f), true))
        {
            for (int32 keyIndex = 0; keyIndex < static_cast<int32>(selectedTrack->keys.size()); ++keyIndex)
            {
                const UIAnimationKeyframe& key = selectedTrack->keys[keyIndex];
                const string label = "Frame " + to_string(key.frame) + "  |  " + Make_KeyValueLabel(*selectedTrack, key);
                if (ImGui::Selectable(label.c_str(), _selectedKeyIndex == keyIndex))
                    Select_Key(_selectedTrackIndex, keyIndex);
            }
        }
        ImGui::EndChild();

        UIAnimationTrack* editableTrack = Get_SelectedTrack();
        UIAnimationKeyframe* selectedKey = Get_SelectedKey();
        if (nullptr == editableTrack || nullptr == selectedKey)
        {
            ImGui::TextDisabled("No key selected.");
            return;
        }

        int32 frame = selectedKey->frame;
        if (ImGui::InputInt("Frame", &frame))
        {
            selectedKey->frame = min(max(frame, _asset.startFrame), _asset.endFrame);
            selectedKey->value = Clamp_UIAnimationTrackValue(editableTrack->type, selectedKey->value);
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

            Set_CurrentFrame(targetFrame);
            Mark_AssetDirty();
        }

        selectedKey = Get_SelectedKey();
        editableTrack = Get_SelectedTrack();
        if (nullptr == selectedKey || nullptr == editableTrack)
            return;

        float value = editableTrack->type == UIAnimationTrackType::Rotation
            ? Radians_ToDegrees(selectedKey->value)
            : selectedKey->value;
        const char* valueLabel = editableTrack->type == UIAnimationTrackType::Rotation ? "Value (deg)" : "Value";
        if (ImGui::DragFloat(valueLabel, &value, Get_ValueDragSpeed(editableTrack->type), 0.f, 0.f, "%.3f"))
        {
            selectedKey->value = editableTrack->type == UIAnimationTrackType::Rotation
                ? Degrees_ToRadians(value)
                : Clamp_UIAnimationTrackValue(editableTrack->type, value);
            Mark_AssetDirty();
            Refresh_StaticPreview();
        }
    }

    void UI_Animation_View::New_Asset()
    {
        _editorManager.Stop_AnimationPreview();
        _asset = {};
        _assetFilePath.clear();
        _assetDirty = false;
        _previewEnabled = true;
        _currentFrame = _asset.startFrame;
        _selectedTrackIndex = -1;
        _selectedKeyIndex = -1;
        _pendingTrackType = UIAnimationTrackType::PositionX;
        Sync_TargetNameBuffer();
    }

    bool UI_Animation_View::Save_Asset(bool forceSaveAs)
    {
        wstring filePath = _assetFilePath;
        if (forceSaveAs || filePath.empty())
        {
            if (!Show_SaveAssetDialog(filePath))
                return false;
        }

        Ensure_AnimationExtension(filePath);
        Clamp_Asset();
        for (UIAnimationTrack& track : _asset.tracks)
            Sort_TrackKeys(track);

        if (!UI_AnimationAssetSerializer::Save_ToJsonFile(_asset, filePath))
            return false;

        _assetFilePath = filePath;
        _assetDirty = false;
        return true;
    }

    bool UI_Animation_View::Load_Asset()
    {
        wstring filePath = {};
        if (!Show_LoadAssetDialog(filePath))
            return false;

        UIAnimationAsset loadedAsset = {};
        if (!UI_AnimationAssetSerializer::Load_FromJsonFile(loadedAsset, filePath))
            return false;

        _editorManager.Stop_AnimationPreview();
        _asset = move(loadedAsset);
        _assetFilePath = filePath;
        _assetDirty = false;
        _selectedTrackIndex = _asset.tracks.empty() ? -1 : 0;
        _selectedKeyIndex = -1;
        Clamp_Asset();
        _currentFrame = _asset.startFrame;
        Resolve_TargetObject();
        Sync_TargetNameBuffer();
        Refresh_StaticPreview();
        return true;
    }

    bool UI_Animation_View::Confirm_DiscardDirtyAsset(const wchar_t* actionName)
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

    bool UI_Animation_View::Show_SaveAssetDialog(wstring& outFilePath) const
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

    bool UI_Animation_View::Show_LoadAssetDialog(wstring& outFilePath) const
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

    wstring UI_Animation_View::Resolve_DefaultAnimationDirectory() const
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

    void UI_Animation_View::Ensure_AnimationExtension(wstring& filePath) const
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

    void UI_Animation_View::Bind_SelectedUI()
    {
        UI_Object* selectedObject = _editorManager.Get_SelectedObject();
        if (nullptr == selectedObject)
            return;

        _asset.targetName = selectedObject->name;
        _asset.targetObjectId = selectedObject->id;
        Sync_TargetNameBuffer();
        Mark_AssetDirty();
        Refresh_StaticPreview();
    }

    void UI_Animation_View::Clear_Target()
    {
        _asset.targetName.clear();
        _asset.targetObjectId = 0;
        Sync_TargetNameBuffer();
        _editorManager.Stop_AnimationPreview();
        Mark_AssetDirty();
    }

    void UI_Animation_View::Resolve_TargetObject()
    {
        if (_asset.targetName.empty())
        {
            _asset.targetObjectId = 0;
            return;
        }

        const UI_Object* currentTarget = _editorManager.Get_Document().Find_Object(_asset.targetObjectId);
        if (nullptr != currentTarget && currentTarget->name == _asset.targetName)
            return;

        _asset.targetObjectId = 0;
        for (const UI_Object& object : _editorManager.Get_Document().Get_Objects())
        {
            if (object.name == _asset.targetName)
            {
                _asset.targetObjectId = object.id;
                return;
            }
        }
    }

    void UI_Animation_View::Sync_TargetNameBuffer()
    {
        memset(_targetNameBuffer, 0, sizeof(_targetNameBuffer));
        strncpy_s(_targetNameBuffer, _asset.targetName.c_str(), _TRUNCATE);
    }

    void UI_Animation_View::Set_CurrentFrame(int32 frame)
    {
        Clamp_Asset();
        _currentFrame = min(max(frame, _asset.startFrame), _asset.endFrame);
        Refresh_StaticPreview();
    }

    void UI_Animation_View::Refresh_StaticPreview()
    {
        Resolve_TargetObject();
        if (!_previewEnabled || _asset.targetObjectId == 0)
            return;

        if (!_editorManager.Is_AnimationPreviewPlaying())
            _editorManager.Preview_AnimationAsset(_asset, _currentFrame);
    }

    void UI_Animation_View::Clamp_Asset()
    {
        _asset.fps = max(1, _asset.fps);
        _asset.endFrame = max(_asset.startFrame + 1, _asset.endFrame);
        _currentFrame = min(max(_currentFrame, _asset.startFrame), _asset.endFrame);

        for (UIAnimationTrack& track : _asset.tracks)
        {
            for (UIAnimationKeyframe& key : track.keys)
            {
                key.frame = min(max(key.frame, _asset.startFrame), _asset.endFrame);
                key.value = Clamp_UIAnimationTrackValue(track.type, key.value);
            }
        }
    }

    void UI_Animation_View::Sort_TrackKeys(UIAnimationTrack& track)
    {
        stable_sort(
            track.keys.begin(),
            track.keys.end(),
            [](const UIAnimationKeyframe& lhs, const UIAnimationKeyframe& rhs)
            {
                return lhs.frame < rhs.frame;
            });

        vector<UIAnimationKeyframe> mergedKeys;
        mergedKeys.reserve(track.keys.size());
        for (const UIAnimationKeyframe& key : track.keys)
        {
            UIAnimationKeyframe normalizedKey = key;
            normalizedKey.frame = min(max(normalizedKey.frame, _asset.startFrame), _asset.endFrame);
            normalizedKey.value = Clamp_UIAnimationTrackValue(track.type, normalizedKey.value);

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

    UIAnimationTrack* UI_Animation_View::Get_SelectedTrack()
    {
        if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
            return nullptr;

        return &_asset.tracks[_selectedTrackIndex];
    }

    const UIAnimationTrack* UI_Animation_View::Get_SelectedTrack() const
    {
        if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
            return nullptr;

        return &_asset.tracks[_selectedTrackIndex];
    }

    UIAnimationKeyframe* UI_Animation_View::Get_SelectedKey()
    {
        UIAnimationTrack* track = Get_SelectedTrack();
        if (nullptr == track || _selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int32>(track->keys.size()))
            return nullptr;

        return &track->keys[_selectedKeyIndex];
    }

    const UIAnimationKeyframe* UI_Animation_View::Get_SelectedKey() const
    {
        const UIAnimationTrack* track = Get_SelectedTrack();
        if (nullptr == track || _selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int32>(track->keys.size()))
            return nullptr;

        return &track->keys[_selectedKeyIndex];
    }

    float UI_Animation_View::Get_TargetCurrentValue(UIAnimationTrackType trackType) const
    {
        const UI_Object* targetObject = _editorManager.Get_Document().Find_Object(_asset.targetObjectId);
        if (nullptr == targetObject)
        {
            if (trackType == UIAnimationTrackType::ScaleX || trackType == UIAnimationTrackType::ScaleY || trackType == UIAnimationTrackType::Alpha)
                return 1.f;

            return 0.f;
        }

        switch (trackType)
        {
        case UIAnimationTrackType::PositionX:
            return targetObject->position.x;
        case UIAnimationTrackType::PositionY:
            return targetObject->position.y;
        case UIAnimationTrackType::ScaleX:
            return targetObject->scale.x;
        case UIAnimationTrackType::ScaleY:
            return targetObject->scale.y;
        case UIAnimationTrackType::Rotation:
            return targetObject->rotationRadians;
        case UIAnimationTrackType::Alpha:
            return targetObject->color.w;
        default:
            return 0.f;
        }
    }

    void UI_Animation_View::Add_Track(UIAnimationTrackType trackType)
    {
        for (int32 index = 0; index < static_cast<int32>(_asset.tracks.size()); ++index)
        {
            if (_asset.tracks[index].type == trackType)
            {
                _selectedTrackIndex = index;
                _selectedKeyIndex = -1;
                return;
            }
        }

        UIAnimationTrack track = {};
        track.type = trackType;
        track.enabled = true;
        _asset.tracks.push_back(track);
        _selectedTrackIndex = static_cast<int32>(_asset.tracks.size()) - 1;
        _selectedKeyIndex = -1;
        Mark_AssetDirty();
    }

    void UI_Animation_View::Delete_SelectedTrack()
    {
        if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
            return;

        _asset.tracks.erase(_asset.tracks.begin() + _selectedTrackIndex);
        if (_selectedTrackIndex >= static_cast<int32>(_asset.tracks.size()))
            _selectedTrackIndex = static_cast<int32>(_asset.tracks.size()) - 1;
        _selectedKeyIndex = -1;
        Mark_AssetDirty();
        Refresh_StaticPreview();
    }

    void UI_Animation_View::Add_KeyToSelectedTrack()
    {
        UIAnimationTrack* track = Get_SelectedTrack();
        if (nullptr == track)
            return;

        const float value = Clamp_UIAnimationTrackValue(track->type, Get_TargetCurrentValue(track->type));
        for (int32 index = 0; index < static_cast<int32>(track->keys.size()); ++index)
        {
            if (track->keys[index].frame == _currentFrame)
            {
                track->keys[index].value = value;
                _selectedKeyIndex = index;
                Mark_AssetDirty();
                Refresh_StaticPreview();
                return;
            }
        }

        UIAnimationKeyframe key = {};
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
        Refresh_StaticPreview();
    }

    void UI_Animation_View::Delete_SelectedKey()
    {
        UIAnimationTrack* track = Get_SelectedTrack();
        if (nullptr == track || _selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int32>(track->keys.size()))
            return;

        track->keys.erase(track->keys.begin() + _selectedKeyIndex);
        if (_selectedKeyIndex >= static_cast<int32>(track->keys.size()))
            _selectedKeyIndex = static_cast<int32>(track->keys.size()) - 1;
        Mark_AssetDirty();
        Refresh_StaticPreview();
    }

    void UI_Animation_View::Select_Key(int32 trackIndex, int32 keyIndex)
    {
        if (trackIndex < 0 || trackIndex >= static_cast<int32>(_asset.tracks.size()))
            return;

        if (keyIndex < 0 || keyIndex >= static_cast<int32>(_asset.tracks[trackIndex].keys.size()))
            return;

        _selectedTrackIndex = trackIndex;
        _selectedKeyIndex = keyIndex;
    }

    void UI_Animation_View::Mark_AssetDirty()
    {
        _assetDirty = true;
    }

    Shared<UI_Animation_View> UI_Animation_View::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Animation_View>(editorManager);
    }
}
