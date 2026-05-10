#include "pch.h"
#include "UI_Animation_View.h"

#include "UI_Editor_Manager.h"

namespace UIEditor
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;

        const char* const kTrackTypeNames[] = {
            "PositionX",
            "PositionY",
            "ScaleX",
            "ScaleY",
            "Rotation",
            "Alpha",
        };

        int32 Get_TrackTypeIndex(UI_AnimationKeyframeTrackType type)
        {
            return min(max(static_cast<int32>(type), 0), static_cast<int32>(size(kTrackTypeNames)) - 1);
        }

        float Get_ValueDragSpeed(UI_AnimationKeyframeTrackType type)
        {
            switch (type)
            {
            case UI_AnimationKeyframeTrackType::PositionX:
            case UI_AnimationKeyframeTrackType::PositionY:
            case UI_AnimationKeyframeTrackType::Rotation:
                return 1.f;
            case UI_AnimationKeyframeTrackType::Alpha:
            case UI_AnimationKeyframeTrackType::ScaleX:
            case UI_AnimationKeyframeTrackType::ScaleY:
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

        string Make_KeyValueLabel(const UI_AnimationKeyframeTrack& track, const UI_AnimationKeyframe& key)
        {
            if (track.type == UI_AnimationKeyframeTrackType::Rotation)
                return to_string(Radians_ToDegrees(key.value)) + " deg";

            return to_string(key.value);
        }
    }

    UI_Animation_View::UI_Animation_View(UI_Editor_Manager& editorManager)
        : UI_Editor_Window{ L"Animation" }
        , _editorManager{ editorManager }
    {
        Sync_TargetNameBuffer();
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

            _assetService.Clamp_Asset();
            _assetService.Resolve_TargetObject(_editorManager.Get_Document());

            const UI_AnimationAsset& asset = _assetService.Get_Asset();
            if (_editorManager.Is_AnimationPreviewActive() && asset.targetObjectId != 0)
                _assetService.Set_CurrentFrame(_editorManager.Get_AnimationPreviewFrame());

            Render_Toolbar();
            ImGui::Separator();
            Render_TargetSection();
            ImGui::Separator();
            Render_PlaybackSection();
            ImGui::Separator();
            Render_TimelineSection();

            if (!_assetService.Is_PreviewEnabled() && _editorManager.Is_AnimationPreviewActive())
                _editorManager.Stop_AnimationPreview();
            else if (_assetService.Is_PreviewEnabled() && !_editorManager.Is_AnimationPreviewPlaying())
                Refresh_StaticPreview();
        }

        Set_Open(isOpen);
        ImGui::End();
    }

    void UI_Animation_View::Render_Toolbar()
    {
        if (ImGui::Button("New") && _assetService.Confirm_DiscardDirtyAsset(L"create a new animation asset"))
        {
            _editorManager.Stop_AnimationPreview();
            _assetService.New_Asset();
            Sync_TargetNameBuffer();
        }

        ImGui::SameLine();
        if (ImGui::Button("Load") && _assetService.Confirm_DiscardDirtyAsset(L"load another animation asset"))
        {
            if (_assetService.Load_Asset())
            {
                _editorManager.Stop_AnimationPreview();
                _assetService.Resolve_TargetObject(_editorManager.Get_Document());
                Sync_TargetNameBuffer();
                Refresh_StaticPreview();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Save"))
            _assetService.Save_Asset(false);

        ImGui::SameLine();
        if (ImGui::Button("Save As"))
            _assetService.Save_Asset(true);

        const UI_AnimationAsset& asset = _assetService.Get_Asset();
        ImGui::SameLine();
        ImGui::TextDisabled(
            "Asset: %s%s",
            asset.name.empty() ? "NewUIAnimation" : asset.name.c_str(),
            _assetService.Is_Dirty() ? " *" : "");
    }

    void UI_Animation_View::Render_TargetSection()
    {
        if (ImGui::Button("Bind Selected UI"))
        {
            if (_assetService.Bind_SelectedUI(_editorManager.Get_SelectedObject()))
            {
                Sync_TargetNameBuffer();
                Refresh_StaticPreview();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Target"))
        {
            _assetService.Clear_Target();
            Sync_TargetNameBuffer();
            _editorManager.Stop_AnimationPreview();
        }

        if (ImGui::InputText("Target Name", _targetNameBuffer, size(_targetNameBuffer)))
        {
            _assetService.Set_TargetName(_targetNameBuffer, _editorManager.Get_Document());
            Refresh_StaticPreview();
        }

        const UI_AnimationAsset& asset = _assetService.Get_Asset();
        const char* boundName = "None";
        if (asset.targetObjectId != 0)
        {
            const UI_Object* targetObject = _editorManager.Get_Document().Find_Object(asset.targetObjectId);
            if (nullptr != targetObject)
                boundName = targetObject->name.c_str();
        }

        ImGui::Text("Bound UI: %s", boundName);
    }

    void UI_Animation_View::Render_PlaybackSection()
    {
        UI_AnimationAsset& asset = _assetService.Get_MutableAsset();
        const bool hasTarget = asset.targetObjectId != 0;

        ImGui::BeginDisabled(!hasTarget);
        if (ImGui::Button("Play"))
            _editorManager.Play_AnimationAssetPreview(asset, _assetService.Get_CurrentFrame());
        ImGui::SameLine();
        if (ImGui::Button("Pause"))
        {
            if (_editorManager.Is_AnimationPreviewActive())
                _assetService.Set_CurrentFrame(_editorManager.Get_AnimationPreviewFrame());
            _editorManager.Pause_AnimationPreview();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            _editorManager.Stop_AnimationPreview();
            Set_CurrentFrame(asset.startFrame);
        }
        ImGui::EndDisabled();

        bool previewEnabled = _assetService.Is_PreviewEnabled();
        ImGui::SameLine();
        if (ImGui::Checkbox("Enable Preview", &previewEnabled))
        {
            _assetService.Set_PreviewEnabled(previewEnabled);
            if (!previewEnabled)
                _editorManager.Stop_AnimationPreview();
            else
                Refresh_StaticPreview();
        }

        bool changed = false;
        ImGui::SetNextItemWidth(78.f);
        changed |= ImGui::InputInt("FPS", &asset.fps);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(78.f);
        changed |= ImGui::InputInt("Start", &asset.startFrame);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(78.f);
        changed |= ImGui::InputInt("End", &asset.endFrame);
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Loop", &asset.loop);

        int32 editableCurrentFrame = _assetService.Get_CurrentFrame();
        ImGui::SetNextItemWidth(78.f);
        if (ImGui::InputInt("Current Frame", &editableCurrentFrame))
            Set_CurrentFrame(editableCurrentFrame);

        if (changed)
        {
            _assetService.Clamp_Asset();
            _assetService.Normalize_AllTracks();
            Set_CurrentFrame(_assetService.Get_CurrentFrame());
            _assetService.Mark_AssetDirty();
        }
    }

    void UI_Animation_View::Render_TimelineSection()
    {
        if (ImGui::BeginTable("AnimationTimelineLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Tracks", ImGuiTableColumnFlags_WidthFixed, UI_AnimationTimelineView::Get_TrackPanelWidth());
            ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, UI_AnimationTimelineView::Get_KeyPanelWidth());
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            Render_TrackPanel();

            ImGui::TableSetColumnIndex(1);
            const ImVec2 timelineSize = ImVec2(ImGui::GetContentRegionAvail().x, max(260.f, ImGui::GetContentRegionAvail().y));
            const UI_AnimationTimelineClickResult clickResult = _timelineView.Render(
                _assetService.Get_Asset(),
                _assetService.Get_CurrentFrame(),
                _assetService.Get_SelectedTrackIndex(),
                _assetService.Get_SelectedKeyIndex(),
                timelineSize);
            Handle_TimelineClick(clickResult);

            ImGui::TableSetColumnIndex(2);
            Render_KeyPanel();

            ImGui::EndTable();
        }
    }

    void UI_Animation_View::Render_TrackPanel()
    {
        const UI_AnimationAsset& asset = _assetService.Get_Asset();

        ImGui::Text("Tracks: %zu", asset.tracks.size());
        ImGui::Separator();

        const float trackListHeight = 130.f;
        if (ImGui::BeginChild("AnimationTrackList", ImVec2(0.f, trackListHeight), true))
        {
            for (int32 index = 0; index < static_cast<int32>(asset.tracks.size()); ++index)
            {
                const UI_AnimationKeyframeTrack& track = asset.tracks[index];
                const string label = string(Get_AnimationKeyframeTrackTypeName(track.type)) + " (" + to_string(track.keys.size()) + ")";
                if (ImGui::Selectable(label.c_str(), _assetService.Get_SelectedTrackIndex() == index))
                    _assetService.Select_Track(index);
            }
        }
        ImGui::EndChild();

        int32 trackTypeIndex = Get_TrackTypeIndex(_assetService.Get_PendingTrackType());
        if (ImGui::Combo("Add Track Type", &trackTypeIndex, kTrackTypeNames, static_cast<int32>(size(kTrackTypeNames))))
            _assetService.Set_PendingTrackType(static_cast<UI_AnimationKeyframeTrackType>(trackTypeIndex));

        if (ImGui::Button("Add Track"))
            _assetService.Add_Track(_assetService.Get_PendingTrackType());

        ImGui::BeginDisabled(nullptr == _assetService.Get_SelectedTrack());
        if (ImGui::Button("Delete Track") && _assetService.Delete_SelectedTrack())
            Refresh_StaticPreview();
        if (ImGui::Button("Add Key To Selected Track") && _assetService.Add_KeyToSelectedTrack(_editorManager.Get_Document()))
            Refresh_StaticPreview();
        ImGui::EndDisabled();

        ImGui::BeginDisabled(nullptr == _assetService.Get_SelectedKey());
        if (ImGui::Button("Delete Key") && _assetService.Delete_SelectedKey())
            Refresh_StaticPreview();
        ImGui::EndDisabled();
    }

    void UI_Animation_View::Render_KeyPanel()
    {
        const UI_AnimationKeyframeTrack* selectedTrack = _assetService.Get_SelectedTrack();
        if (nullptr == selectedTrack)
        {
            ImGui::TextDisabled("No track selected.");
            return;
        }

        ImGui::Text("Track: %s", Get_AnimationKeyframeTrackTypeName(selectedTrack->type));
        ImGui::Text("Keys: %zu", selectedTrack->keys.size());
        ImGui::Separator();

        if (ImGui::BeginChild("AnimationKeyList", ImVec2(0.f, 120.f), true))
        {
            for (int32 keyIndex = 0; keyIndex < static_cast<int32>(selectedTrack->keys.size()); ++keyIndex)
            {
                const UI_AnimationKeyframe& key = selectedTrack->keys[keyIndex];
                const string label = "Frame " + to_string(key.frame) + "  |  " + Make_KeyValueLabel(*selectedTrack, key);
                if (ImGui::Selectable(label.c_str(), _assetService.Get_SelectedKeyIndex() == keyIndex))
                    _assetService.Select_Key(_assetService.Get_SelectedTrackIndex(), keyIndex);
            }
        }
        ImGui::EndChild();

        UI_AnimationKeyframeTrack* editableTrack = _assetService.Get_SelectedTrack();
        UI_AnimationKeyframe* selectedKey = _assetService.Get_SelectedKey();
        if (nullptr == editableTrack || nullptr == selectedKey)
        {
            ImGui::TextDisabled("No key selected.");
            return;
        }

        int32 frame = selectedKey->frame;
        if (ImGui::InputInt("Frame", &frame))
        {
            int32 targetFrame = frame;
            if (_assetService.Set_SelectedKeyFrame(frame, targetFrame))
                Set_CurrentFrame(targetFrame);
        }

        selectedKey = _assetService.Get_SelectedKey();
        editableTrack = _assetService.Get_SelectedTrack();
        if (nullptr == selectedKey || nullptr == editableTrack)
            return;

        float value = editableTrack->type == UI_AnimationKeyframeTrackType::Rotation
            ? Radians_ToDegrees(selectedKey->value)
            : selectedKey->value;
        const char* valueLabel = editableTrack->type == UI_AnimationKeyframeTrackType::Rotation ? "Value (deg)" : "Value";
        if (ImGui::DragFloat(valueLabel, &value, Get_ValueDragSpeed(editableTrack->type), 0.f, 0.f, "%.3f"))
        {
            const float nextValue = editableTrack->type == UI_AnimationKeyframeTrackType::Rotation
                ? Degrees_ToRadians(value)
                : value;
            if (_assetService.Set_SelectedKeyValue(nextValue))
                Refresh_StaticPreview();
        }
    }

    void UI_Animation_View::Sync_TargetNameBuffer()
    {
        memset(_targetNameBuffer, 0, sizeof(_targetNameBuffer));
        strncpy_s(_targetNameBuffer, _assetService.Get_Asset().targetName.c_str(), _TRUNCATE);
    }

    void UI_Animation_View::Set_CurrentFrame(int32 frame)
    {
        _assetService.Set_CurrentFrame(frame);
        Refresh_StaticPreview();
    }

    void UI_Animation_View::Refresh_StaticPreview()
    {
        _assetService.Resolve_TargetObject(_editorManager.Get_Document());
        const UI_AnimationAsset& asset = _assetService.Get_Asset();
        if (!_assetService.Is_PreviewEnabled() || asset.targetObjectId == 0)
            return;

        if (!_editorManager.Is_AnimationPreviewPlaying())
            _editorManager.Preview_AnimationAsset(asset, _assetService.Get_CurrentFrame());
    }

    void UI_Animation_View::Handle_TimelineClick(const UI_AnimationTimelineClickResult& clickResult)
    {
        if (!clickResult.clicked)
            return;

        if (clickResult.trackIndex >= 0)
        {
            if (clickResult.hitKey)
                _assetService.Select_Key(clickResult.trackIndex, clickResult.keyIndex);
            else
                _assetService.Select_Track(clickResult.trackIndex);
        }

        Set_CurrentFrame(clickResult.frame);
    }

    Shared<UI_Animation_View> UI_Animation_View::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Animation_View>(editorManager);
    }
}
