#include "pch.h"
#include "UI_Animation_View.h"

#include "UI_Editor_Manager.h"

namespace UIEditor
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;

        const char* const kEasingNames[] = {
            "Linear",
            "EaseIn",
            "EaseOut",
            "EaseInOut",
        };

        const char* const kAnimationAssetTypeNames[] = {
            "Target_Animation",
            "Public_Animation",
        };

        int32 Get_AnimationAssetTypeIndex(UI_AnimationAssetType type)
        {
            return min(max(static_cast<int32>(type), 0), static_cast<int32>(size(kAnimationAssetTypeNames)) - 1);
        }

        int32 Get_EasingIndex(UI_AnimationEasing easing)
        {
            return min(max(static_cast<int32>(easing), 0), static_cast<int32>(size(kEasingNames)) - 1);
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

            if (track.type == UI_AnimationKeyframeTrackType::Size)
            {
                return
                    "Size(" +
                    to_string(key.valueVec2.x) + ", " +
                    to_string(key.valueVec2.y) + ")";
            }

            if (track.type == UI_AnimationKeyframeTrackType::Color)
            {
                const ImVec4 color = Clamp_AnimationKeyframeColor(key.color);
                return
                    "RGBA(" +
                    to_string(static_cast<int32>(roundf(color.x))) + ", " +
                    to_string(static_cast<int32>(roundf(color.y))) + ", " +
                    to_string(static_cast<int32>(roundf(color.z))) + ", " +
                    to_string(static_cast<int32>(roundf(color.w))) + ")";
            }

            return to_string(key.value);
        }

        size_t Count_Tracks(const UI_AnimationAsset& asset)
        {
            if (Is_PublicAnimation(asset))
                return asset.tracks.size();

            size_t trackCount = 0;
            for (const UI_AnimationTargetBinding& target : asset.targets)
                trackCount += target.tracks.size();

            return trackCount;
        }

        void Show_ItemTooltip(const char* text)
        {
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", text);
        }
    }

    void UI_Animation_View::Render_KeyframeAnimationEditor()
    {
        if (_editorManager.Is_AnimationPreviewActive() && _assetService.Has_AnyResolvedTarget())
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

    void UI_Animation_View::Render_Toolbar()
    {
        if (ImGui::Button("New") && _assetService.Confirm_DiscardDirtyAsset(L"create a new animation asset"))
        {
            _editorManager.Stop_AnimationPreview();
            _assetService.New_Asset();
            Mark_StaticPreviewDirty();
            Sync_AssetNameBuffer();
            Sync_TargetNameBuffer();
        }

        ImGui::SameLine();
        if (ImGui::Button("Load") && _assetService.Confirm_DiscardDirtyAsset(L"load another animation asset"))
        {
            if (_assetService.Load_Asset())
            {
                _editorManager.Stop_AnimationPreview();
                _assetService.Resolve_TargetObject(_editorManager.Get_Document(), true);
                Sync_AssetNameBuffer();
                Sync_TargetNameBuffer();
                Request_StaticPreviewRefresh();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Save"))
            _assetService.Save_Asset(false);

        ImGui::SameLine();
        if (ImGui::Button("Save As"))
            _assetService.Save_Asset(true);

        ImGui::SameLine();
        ImGui::BeginDisabled(!_assetService.Can_UndoAssetEdit());
        if (ImGui::Button("Undo"))
            Undo_KeyframeAnimationEdit();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!_assetService.Can_RedoAssetEdit());
        if (ImGui::Button("Redo"))
            Redo_KeyframeAnimationEdit();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.f);
        if (ImGui::InputText("Asset Name", _assetNameBuffer, size(_assetNameBuffer)))
            _assetService.Set_AssetName(_assetNameBuffer);

        if (_assetService.Is_Dirty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("*");
        }

        UI_AnimationAsset& asset = _assetService.Get_MutableAsset();
        int32 animationTypeIndex = Get_AnimationAssetTypeIndex(asset.animationType);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(172.f);
        if (ImGui::Combo("Animation Type", &animationTypeIndex, kAnimationAssetTypeNames, static_cast<int32>(size(kAnimationAssetTypeNames))))
        {
            _editorManager.Stop_AnimationPreview();
            _assetService.Set_AnimationType(static_cast<UI_AnimationAssetType>(animationTypeIndex));
            Request_StaticPreviewRefresh();
        }
    }

    void UI_Animation_View::Render_TargetSection()
    {
        const bool isPublicAnimation = _assetService.Is_PublicAnimationMode();
        if (ImGui::Button(isPublicAnimation ? "Use Selected UI As Preview Target" : "Bind Selected UI"))
        {
            if (_assetService.Bind_SelectedUI(_editorManager.Get_SelectedObject()))
            {
                Sync_TargetNameBuffer();
                Request_StaticPreviewRefresh();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(isPublicAnimation ? "Clear Preview Target" : "Remove Target"))
        {
            if (_assetService.Remove_SelectedTarget())
            {
                Sync_TargetNameBuffer();
                _editorManager.Stop_AnimationPreview();
                Request_StaticPreviewRefresh();
            }
        }

        ImGui::BeginDisabled(nullptr == _assetService.Get_SelectedTarget() && !isPublicAnimation);
        if (ImGui::InputText(isPublicAnimation ? "Preview Target Name" : "Target Name", _targetNameBuffer, size(_targetNameBuffer)))
        {
            _assetService.Set_TargetName(_targetNameBuffer, _editorManager.Get_Document());
            Request_StaticPreviewRefresh();
        }
        ImGui::EndDisabled();

        const char* boundName = "None";
        const UI_AnimationTargetBinding* selectedTarget = _assetService.Get_SelectedTarget();
        if (nullptr != selectedTarget && selectedTarget->targetObjectId != 0)
        {
            const UI_Object* targetObject = _editorManager.Get_Document().Find_Object(selectedTarget->targetObjectId);
            if (nullptr != targetObject)
                boundName = targetObject->name.c_str();
        }

        ImGui::Text("%s: %s", isPublicAnimation ? "Preview UI" : "Bound UI", boundName);
    }

    void UI_Animation_View::Render_PlaybackSection()
    {
        UI_AnimationAsset& asset = _assetService.Get_MutableAsset();
        const bool hasTarget = _assetService.Has_AnyResolvedTarget();

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
                Request_StaticPreviewRefresh();
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
                _assetService.Get_SelectedTargetIndex(),
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
        const bool isPublicAnimation = Is_PublicAnimation(asset);
        const bool hasTarget = isPublicAnimation || nullptr != _assetService.Get_SelectedTarget();
        const bool hasResolvedTarget = _assetService.Has_SelectedResolvedTarget();
        const bool canKeyCurrentValue = isPublicAnimation || hasResolvedTarget;

        ImGui::Text("Targets: %zu  Tracks: %zu", asset.targets.size(), Count_Tracks(asset));
        ImGui::Separator();

        const float trackListHeight = 170.f;
        const float deleteButtonWidth = 54.f;
        int32 pendingDeleteTargetIndex = INT_MIN;
        int32 pendingDeleteTrackIndex = -1;
        if (ImGui::BeginChild("AnimationTrackList", ImVec2(0.f, trackListHeight), true))
        {
            if (isPublicAnimation)
            {
                ImGui::TextDisabled("Public Tracks");
                for (int32 trackIndex = 0; trackIndex < static_cast<int32>(asset.tracks.size()); ++trackIndex)
                {
                    const UI_AnimationKeyframeTrack& track = asset.tracks[trackIndex];
                    const string label =
                        string(Get_AnimationKeyframeTrackTypeName(track.type)) +
                        " (" +
                        to_string(track.keys.size()) +
                        ")##PublicTrack" +
                        to_string(trackIndex);
                    const float selectableWidth = max(1.f, ImGui::GetContentRegionAvail().x - deleteButtonWidth - ImGui::GetStyle().ItemSpacing.x);
                    if (ImGui::Selectable(label.c_str(), _assetService.Get_SelectedTrackIndex() == trackIndex, 0, ImVec2(selectableWidth, 0.f)))
                    {
                        _assetService.Select_Track(-1, trackIndex);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton(("Delete##PublicTrackDelete" + to_string(trackIndex)).c_str()))
                    {
                        pendingDeleteTargetIndex = -1;
                        pendingDeleteTrackIndex = trackIndex;
                    }
                }
            }
            else
            {
                for (int32 targetIndex = 0; targetIndex < static_cast<int32>(asset.targets.size()); ++targetIndex)
                {
                    const UI_AnimationTargetBinding& target = asset.targets[targetIndex];
                    const string targetName = target.targetName.empty() ? string("(Unnamed Target)") : target.targetName;
                    const string targetLabel = targetName + "##Target" + to_string(targetIndex);
                    const bool selectedTargetOnly =
                        _assetService.Get_SelectedTargetIndex() == targetIndex &&
                        _assetService.Get_SelectedTrackIndex() < 0;
                    if (ImGui::Selectable(targetLabel.c_str(), selectedTargetOnly))
                    {
                        _assetService.Select_Target(targetIndex);
                        Sync_TargetNameBuffer();
                    }

                    for (int32 trackIndex = 0; trackIndex < static_cast<int32>(target.tracks.size()); ++trackIndex)
                    {
                        const UI_AnimationKeyframeTrack& track = target.tracks[trackIndex];
                        const string label =
                            "  " +
                            string(Get_AnimationKeyframeTrackTypeName(track.type)) +
                            " (" +
                            to_string(track.keys.size()) +
                            ")##Track" +
                            to_string(targetIndex) +
                            "_" +
                            to_string(trackIndex);
                        const bool selectedTrack =
                            _assetService.Get_SelectedTargetIndex() == targetIndex &&
                            _assetService.Get_SelectedTrackIndex() == trackIndex;
                        const float selectableWidth = max(1.f, ImGui::GetContentRegionAvail().x - deleteButtonWidth - ImGui::GetStyle().ItemSpacing.x);
                        if (ImGui::Selectable(label.c_str(), selectedTrack, 0, ImVec2(selectableWidth, 0.f)))
                        {
                            _assetService.Select_Track(targetIndex, trackIndex);
                            Sync_TargetNameBuffer();
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton(("Delete##TrackDelete" + to_string(targetIndex) + "_" + to_string(trackIndex)).c_str()))
                        {
                            pendingDeleteTargetIndex = targetIndex;
                            pendingDeleteTrackIndex = trackIndex;
                        }
                    }
                }
            }
        }
        ImGui::EndChild();

        if (pendingDeleteTrackIndex >= 0)
        {
            _assetService.Select_Track(pendingDeleteTargetIndex, pendingDeleteTrackIndex);
            if (_assetService.Delete_SelectedTrack())
                Request_StaticPreviewRefresh();
        }

        int32 trackTypeIndex = Get_AnimationKeyframeTrackAddOptionIndex(_assetService.Get_PendingTrackType());
        const int32 trackTypeOptionCount = Get_AnimationKeyframeTrackAddOptionCount();
        if (ImGui::BeginCombo("Add Track Type", Get_AnimationKeyframeTrackAddOption(trackTypeIndex).name))
        {
            for (int32 optionIndex = 0; optionIndex < trackTypeOptionCount; ++optionIndex)
            {
                const UI_AnimationKeyframeTrackMetadata& metadata = Get_AnimationKeyframeTrackAddOption(optionIndex);
                const bool selected = optionIndex == trackTypeIndex;
                if (ImGui::Selectable(metadata.name, selected))
                {
                    trackTypeIndex = optionIndex;
                    _assetService.Set_PendingTrackType(metadata.type);
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::BeginDisabled(!hasTarget);
        if (ImGui::Button("Add Track"))
            _assetService.Add_Track(_assetService.Get_PendingTrackType(), &_editorManager.Get_Document());
        ImGui::EndDisabled();

        UI_AnimationKeyframeTrack* selectedTrack = _assetService.Get_SelectedTrack();
        ImGui::BeginDisabled(nullptr == selectedTrack);
        int32 trackEasingIndex = nullptr != selectedTrack ? Get_EasingIndex(selectedTrack->easing) : 0;
        if (ImGui::Combo("Track Easing", &trackEasingIndex, kEasingNames, static_cast<int32>(size(kEasingNames))) &&
            _assetService.Set_SelectedTrackEasing(static_cast<UI_AnimationEasing>(trackEasingIndex), true))
        {
            Request_StaticPreviewRefresh();
        }
        Show_ItemTooltip("Applies the easing to every key on the selected track.");
        ImGui::EndDisabled();

        ImGui::BeginDisabled(nullptr == _assetService.Get_SelectedTrack());
        if (ImGui::Button("Delete Track") && _assetService.Delete_SelectedTrack())
            Request_StaticPreviewRefresh();
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!canKeyCurrentValue || nullptr == _assetService.Get_SelectedTrack());
        if (ImGui::Button("Key Selected Track") && _assetService.Add_KeyToSelectedTrack(_editorManager.Get_Document()))
            Request_StaticPreviewRefresh();
        ImGui::EndDisabled();
        Show_ItemTooltip("Create or update one key from the bound UI at the current frame.");

        ImGui::BeginDisabled(!canKeyCurrentValue);
        if (ImGui::Button("Key Transform Set") && _assetService.Add_CurrentValueKeysToTransformTracks(_editorManager.Get_Document()))
            Request_StaticPreviewRefresh();
        ImGui::EndDisabled();
        Show_ItemTooltip("Create or update Position, Size, Scale, Rotation, Color, and AlphaMultiplier keys at the current frame.");

        ImGui::BeginDisabled(nullptr == _assetService.Get_SelectedKey());
        if (ImGui::Button("Copy Key"))
            _assetService.Copy_SelectedKey();
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!_assetService.Has_CopiedKey());
        if (ImGui::Button("Paste Key") && _assetService.Paste_CopiedKey())
            Request_StaticPreviewRefresh();
        ImGui::EndDisabled();

        ImGui::BeginDisabled(nullptr == _assetService.Get_SelectedKey());
        if (ImGui::Button("Delete Key") && _assetService.Delete_SelectedKey())
            Request_StaticPreviewRefresh();
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

        int32 pendingDeleteKeyIndex = -1;
        if (ImGui::BeginChild("AnimationKeyList", ImVec2(0.f, 120.f), true))
        {
            for (int32 keyIndex = 0; keyIndex < static_cast<int32>(selectedTrack->keys.size()); ++keyIndex)
            {
                const UI_AnimationKeyframe& key = selectedTrack->keys[keyIndex];
                const string label = "Frame " + to_string(key.frame) + "  |  " + Make_KeyValueLabel(*selectedTrack, key);
                const float deleteButtonWidth = 54.f;
                const float selectableWidth = max(1.f, ImGui::GetContentRegionAvail().x - deleteButtonWidth - ImGui::GetStyle().ItemSpacing.x);
                if (ImGui::Selectable(label.c_str(), _assetService.Get_SelectedKeyIndex() == keyIndex, 0, ImVec2(selectableWidth, 0.f)))
                    _assetService.Select_Key(_assetService.Get_SelectedTargetIndex(), _assetService.Get_SelectedTrackIndex(), keyIndex);
                ImGui::SameLine();
                if (ImGui::SmallButton(("Delete##KeyDelete" + to_string(keyIndex)).c_str()))
                    pendingDeleteKeyIndex = keyIndex;
            }
        }
        ImGui::EndChild();

        if (pendingDeleteKeyIndex >= 0)
        {
            if (_assetService.Select_Key(_assetService.Get_SelectedTargetIndex(), _assetService.Get_SelectedTrackIndex(), pendingDeleteKeyIndex) &&
                _assetService.Delete_SelectedKey())
            {
                Request_StaticPreviewRefresh();
            }
            return;
        }

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
            if (_assetService.TryMoveSelectedKeyFrame(frame, targetFrame))
                Set_CurrentFrame(targetFrame);
        }

        selectedKey = _assetService.Get_SelectedKey();
        editableTrack = _assetService.Get_SelectedTrack();
        if (nullptr == selectedKey || nullptr == editableTrack)
            return;

        int32 keyEasingIndex = Get_EasingIndex(selectedKey->easing);
        if (ImGui::Combo("Key Easing", &keyEasingIndex, kEasingNames, static_cast<int32>(size(kEasingNames))) &&
            _assetService.Set_SelectedKeyEasing(static_cast<UI_AnimationEasing>(keyEasingIndex)))
        {
            Request_StaticPreviewRefresh();
        }
        Show_ItemTooltip("Controls interpolation from this key to the next key.");

        if (editableTrack->type == UI_AnimationKeyframeTrackType::Color)
        {
            const ImVec4 color = Clamp_AnimationKeyframeColor(selectedKey->color);
            int32 channels[4] = {
                static_cast<int32>(roundf(color.x)),
                static_cast<int32>(roundf(color.y)),
                static_cast<int32>(roundf(color.z)),
                static_cast<int32>(roundf(color.w)),
            };

            if (ImGui::InputInt4("RGBA", channels))
            {
                const ImVec4 nextColor(
                    static_cast<float>(min(max(channels[0], 0), 255)),
                    static_cast<float>(min(max(channels[1], 0), 255)),
                    static_cast<float>(min(max(channels[2], 0), 255)),
                    static_cast<float>(min(max(channels[3], 0), 255)));
                if (_assetService.Set_SelectedKeyColor(nextColor))
                    Request_StaticPreviewRefresh();
            }
            Show_ItemTooltip("Edit R, G, B, and A as 0 to 255 values.");
        }
        else if (editableTrack->type == UI_AnimationKeyframeTrackType::Size)
        {
            ImVec2 value = selectedKey->valueVec2;
            if (ImGui::DragFloat2("Size", &value.x, Get_AnimationKeyframeTrackDragSpeed(editableTrack->type), 0.f, 0.f, "%.3f"))
            {
                if (_assetService.Set_SelectedKeyVec2Value(value))
                    Request_StaticPreviewRefresh();
            }
        }
        else
        {
            float value = editableTrack->type == UI_AnimationKeyframeTrackType::Rotation
                ? Radians_ToDegrees(selectedKey->value)
                : selectedKey->value;
            const char* valueLabel = editableTrack->type == UI_AnimationKeyframeTrackType::Rotation ? "Value (deg)" : "Value";
            if (ImGui::DragFloat(valueLabel, &value, Get_AnimationKeyframeTrackDragSpeed(editableTrack->type), 0.f, 0.f, "%.3f"))
            {
                const float nextValue = editableTrack->type == UI_AnimationKeyframeTrackType::Rotation
                    ? Degrees_ToRadians(value)
                    : value;
                if (_assetService.Set_SelectedKeyValue(nextValue))
                    Request_StaticPreviewRefresh();
            }
        }
    }
}
