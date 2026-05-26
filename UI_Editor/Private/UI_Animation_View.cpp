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
            "Color",
        };

        const char* const kEasingNames[] = {
            "Linear",
            "EaseIn",
            "EaseOut",
            "EaseInOut",
        };

        int32 Get_TrackTypeIndex(UI_AnimationKeyframeTrackType type)
        {
            if (type == UI_AnimationKeyframeTrackType::Alpha)
                return static_cast<int32>(UI_AnimationKeyframeTrackType::Color);

            return min(max(static_cast<int32>(type), 0), static_cast<int32>(size(kTrackTypeNames)) - 1);
        }

        int32 Get_EasingIndex(UI_AnimationEasing easing)
        {
            return min(max(static_cast<int32>(easing), 0), static_cast<int32>(size(kEasingNames)) - 1);
        }

        float Get_ValueDragSpeed(UI_AnimationKeyframeTrackType type)
        {
            switch (type)
            {
            case UI_AnimationKeyframeTrackType::PositionX:
            case UI_AnimationKeyframeTrackType::PositionY:
            case UI_AnimationKeyframeTrackType::Rotation:
                return 1.f;
            case UI_AnimationKeyframeTrackType::Color:
                return 0.f;
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

    UI_Animation_View::UI_Animation_View(UI_Editor_Manager& editorManager)
        : UI_Editor_Window{ L"Animation" }
        , _editorManager{ editorManager }
    {
        Sync_AssetNameBuffer();
        Sync_TargetNameBuffer();
    }

    void UI_Animation_View::Render()
    {
        if (!Is_Open())
        {
            _isFocused = false;
            _isHovered = false;
            return;
        }

        bool isOpen = Is_Open();
        const string windowName = String::ToString(Get_WindowName());
        _isFocused = false;
        _isHovered = false;

        if (ImGui::Begin(windowName.c_str(), &isOpen))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
            Handle_Shortcuts();

            _assetService.Clamp_Asset();
            _assetService.Resolve_TargetObject(_editorManager.Get_Document());

            if (_editorManager.Is_AnimationPreviewActive() && _assetService.Has_AnyResolvedTarget())
                _assetService.Set_CurrentFrame(_editorManager.Get_AnimationPreviewFrame());

            Render_Toolbar();
            ImGui::Separator();
            Render_TargetSection();
            ImGui::Separator();
            Render_PlaybackSection();
            ImGui::Separator();
            Render_PresetPanel();
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
            Sync_AssetNameBuffer();
            Sync_TargetNameBuffer();
        }

        ImGui::SameLine();
        if (ImGui::Button("Load") && _assetService.Confirm_DiscardDirtyAsset(L"load another animation asset"))
        {
            if (_assetService.Load_Asset())
            {
                _editorManager.Stop_AnimationPreview();
                _assetService.Resolve_TargetObject(_editorManager.Get_Document());
                Sync_AssetNameBuffer();
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

        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.f);
        if (ImGui::InputText("Asset Name", _assetNameBuffer, size(_assetNameBuffer)))
            _assetService.Set_AssetName(_assetNameBuffer);

        if (_assetService.Is_Dirty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("*");
        }
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
        if (ImGui::Button("Remove Target"))
        {
            if (_assetService.Remove_SelectedTarget())
            {
                Sync_TargetNameBuffer();
                _editorManager.Stop_AnimationPreview();
                Refresh_StaticPreview();
            }
        }

        ImGui::BeginDisabled(nullptr == _assetService.Get_SelectedTarget());
        if (ImGui::InputText("Target Name", _targetNameBuffer, size(_targetNameBuffer)))
        {
            _assetService.Set_TargetName(_targetNameBuffer, _editorManager.Get_Document());
            Refresh_StaticPreview();
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

        ImGui::Text("Bound UI: %s", boundName);
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

    void UI_Animation_View::Render_PresetPanel()
    {
        const bool hasTarget = _assetService.Has_SelectedResolvedTarget();

        ImGui::Text("Presets");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(78.f);
        ImGui::DragInt("Duration", &_presetDurationFrames, 1.f, 1, 240);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(78.f);
        ImGui::DragFloat("Distance", &_presetDistance, 1.f, 1.f, 400.f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(78.f);
        ImGui::DragFloat("Scale", &_presetScaleAmount, 0.01f, 0.01f, 2.f, "%.2f");

        _presetDurationFrames = min(max(_presetDurationFrames, 1), 240);
        _presetDistance = max(1.f, _presetDistance);
        _presetScaleAmount = max(0.01f, _presetScaleAmount);

        auto presetButton = [&](const char* label, UI_AnimationPresetType presetType)
        {
            if (ImGui::Button(label) &&
                _assetService.Apply_Preset(_editorManager.Get_Document(), presetType, _presetDurationFrames, _presetDistance, _presetScaleAmount))
            {
                Refresh_StaticPreview();
            }
        };

        ImGui::BeginDisabled(!hasTarget);
        presetButton("Fade In", UI_AnimationPresetType::FadeIn);
        ImGui::SameLine();
        presetButton("Fade Out", UI_AnimationPresetType::FadeOut);
        ImGui::SameLine();
        presetButton("Pop In", UI_AnimationPresetType::PopIn);
        ImGui::SameLine();
        presetButton("Tooltip Appear", UI_AnimationPresetType::TooltipAppear);
        ImGui::SameLine();
        presetButton("Skill Node Reveal", UI_AnimationPresetType::SkillNodeReveal);

        presetButton("Slide In Up", UI_AnimationPresetType::SlideInUp);
        ImGui::SameLine();
        presetButton("Slide In Down", UI_AnimationPresetType::SlideInDown);
        ImGui::SameLine();
        presetButton("Slide In Left", UI_AnimationPresetType::SlideInLeft);
        ImGui::SameLine();
        presetButton("Slide In Right", UI_AnimationPresetType::SlideInRight);
        ImGui::SameLine();
        presetButton("Button Hover Pulse", UI_AnimationPresetType::ButtonHoverPulse);
        ImGui::SameLine();
        presetButton("Button Press Pop", UI_AnimationPresetType::ButtonPressPop);
        ImGui::EndDisabled();
    }

    void UI_Animation_View::Render_TrackPanel()
    {
        const UI_AnimationAsset& asset = _assetService.Get_Asset();
        const bool hasTarget = nullptr != _assetService.Get_SelectedTarget();
        const bool hasResolvedTarget = _assetService.Has_SelectedResolvedTarget();

        ImGui::Text("Targets: %zu  Tracks: %zu", asset.targets.size(), Count_Tracks(asset));
        ImGui::Separator();

        const float trackListHeight = 170.f;
        if (ImGui::BeginChild("AnimationTrackList", ImVec2(0.f, trackListHeight), true))
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
                    if (ImGui::Selectable(label.c_str(), selectedTrack))
                    {
                        _assetService.Select_Track(targetIndex, trackIndex);
                        Sync_TargetNameBuffer();
                    }
                }
            }
        }
        ImGui::EndChild();

        int32 trackTypeIndex = Get_TrackTypeIndex(_assetService.Get_PendingTrackType());
        if (ImGui::Combo("Add Track Type", &trackTypeIndex, kTrackTypeNames, static_cast<int32>(size(kTrackTypeNames))))
            _assetService.Set_PendingTrackType(static_cast<UI_AnimationKeyframeTrackType>(trackTypeIndex));

        ImGui::BeginDisabled(!hasTarget);
        if (ImGui::Button("Add Track"))
            _assetService.Add_Track(_assetService.Get_PendingTrackType());
        ImGui::EndDisabled();

        UI_AnimationKeyframeTrack* selectedTrack = _assetService.Get_SelectedTrack();
        ImGui::BeginDisabled(nullptr == selectedTrack);
        int32 trackEasingIndex = nullptr != selectedTrack ? Get_EasingIndex(selectedTrack->easing) : 0;
        if (ImGui::Combo("Track Easing", &trackEasingIndex, kEasingNames, static_cast<int32>(size(kEasingNames))) &&
            _assetService.Set_SelectedTrackEasing(static_cast<UI_AnimationEasing>(trackEasingIndex), true))
        {
            Refresh_StaticPreview();
        }
        Show_ItemTooltip("Applies the easing to every key on the selected track.");
        ImGui::EndDisabled();

        ImGui::BeginDisabled(nullptr == _assetService.Get_SelectedTrack());
        if (ImGui::Button("Delete Track") && _assetService.Delete_SelectedTrack())
            Refresh_StaticPreview();
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!hasResolvedTarget || nullptr == _assetService.Get_SelectedTrack());
        if (ImGui::Button("Key Selected Track") && _assetService.Add_KeyToSelectedTrack(_editorManager.Get_Document()))
            Refresh_StaticPreview();
        ImGui::EndDisabled();
        Show_ItemTooltip("Create or update one key from the bound UI at the current frame.");

        ImGui::BeginDisabled(!hasResolvedTarget);
        if (ImGui::Button("Key Transform Set") && _assetService.Add_CurrentValueKeysToTransformTracks(_editorManager.Get_Document()))
            Refresh_StaticPreview();
        ImGui::EndDisabled();
        Show_ItemTooltip("Create or update Position, Scale, Rotation, and Color keys at the current frame.");

        ImGui::BeginDisabled(nullptr == _assetService.Get_SelectedKey());
        if (ImGui::Button("Copy Key"))
            _assetService.Copy_SelectedKey();
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!_assetService.Has_CopiedKey());
        if (ImGui::Button("Paste Key") && _assetService.Paste_CopiedKey())
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
                    _assetService.Select_Key(_assetService.Get_SelectedTargetIndex(), _assetService.Get_SelectedTrackIndex(), keyIndex);
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
            Refresh_StaticPreview();
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
                    Refresh_StaticPreview();
            }
            Show_ItemTooltip("Edit R, G, B, and A as 0 to 255 values.");
        }
        else
        {
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
    }

    void UI_Animation_View::Handle_Shortcuts()
    {
        if (!_isFocused)
            return;

        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput ||
            ImGui::IsAnyItemActive() ||
            ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup))
        {
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
        {
            _assetService.Copy_SelectedKey();
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
        {
            if (_assetService.Paste_CopiedKey())
                Refresh_StaticPreview();
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
        {
            if (_assetService.Delete_SelectedKey())
                Refresh_StaticPreview();
            return;
        }
    }

    void UI_Animation_View::Sync_AssetNameBuffer()
    {
        memset(_assetNameBuffer, 0, sizeof(_assetNameBuffer));
        strncpy_s(_assetNameBuffer, _assetService.Get_Asset().name.c_str(), _TRUNCATE);
    }

    void UI_Animation_View::Sync_TargetNameBuffer()
    {
        memset(_targetNameBuffer, 0, sizeof(_targetNameBuffer));
        const UI_AnimationTargetBinding* target = _assetService.Get_SelectedTarget();
        if (nullptr != target)
            strncpy_s(_targetNameBuffer, target->targetName.c_str(), _TRUNCATE);
    }

    void UI_Animation_View::Set_CurrentFrame(int32 frame)
    {
        _assetService.Set_CurrentFrame(frame);
        Refresh_StaticPreview();
    }

    void UI_Animation_View::Refresh_StaticPreview()
    {
        _assetService.Resolve_TargetObject(_editorManager.Get_Document());
        if (!_assetService.Is_PreviewEnabled() || !_assetService.Has_AnyResolvedTarget())
            return;

        if (!_editorManager.Is_AnimationPreviewPlaying())
            _editorManager.Preview_AnimationAsset(_assetService.Get_Asset(), _assetService.Get_CurrentFrame());
    }

    void UI_Animation_View::Handle_TimelineClick(const UI_AnimationTimelineClickResult& clickResult)
    {
        if (!clickResult.clicked)
            return;

        bool selectedKey = false;
        if (clickResult.targetIndex >= 0 && clickResult.trackIndex >= 0)
        {
            if (clickResult.hitKey)
                selectedKey = _assetService.Select_Key(clickResult.targetIndex, clickResult.trackIndex, clickResult.keyIndex);
            else
                _assetService.Select_Track(clickResult.targetIndex, clickResult.trackIndex);
            Sync_TargetNameBuffer();
        }
        else if (clickResult.targetIndex >= 0)
        {
            _assetService.Select_Target(clickResult.targetIndex);
            Sync_TargetNameBuffer();
        }

        if (clickResult.draggedKey)
        {
            if (!selectedKey)
                return;

            int32 targetFrame = clickResult.frame;
            if (_assetService.TryMoveSelectedKeyFrame(clickResult.frame, targetFrame))
                Set_CurrentFrame(targetFrame);
            return;
        }

        Set_CurrentFrame(clickResult.frame);
    }

    Shared<UI_Animation_View> UI_Animation_View::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Animation_View>(editorManager);
    }
}
