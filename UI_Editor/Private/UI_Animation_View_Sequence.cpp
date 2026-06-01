#include "pch.h"
#include "UI_Animation_View.h"

#include "UI_Editor_Manager.h"

namespace UIEditor
{
    namespace
    {
        string Make_ClipFileName(const UI_AnimationSequenceClip& clip)
        {
            if (clip.filePath.empty())
                return "";

            return String::ToString(fs::path(clip.filePath).filename().wstring());
        }
    }

    void UI_Animation_View::Render_SequencePreviewEditor()
    {
        if (_editorManager.Is_AnimationPreviewActive() && _sequenceService.Has_AnyResolvedTarget())
            _sequenceService.Set_CurrentFrame(_editorManager.Get_AnimationPreviewFrame());

        Render_SequenceToolbar();
        ImGui::Separator();
        Render_SequenceClipList();
        ImGui::Separator();
        Render_SequenceTimelineSection();

        if (_sequenceService.Is_Empty() || !_sequenceService.Has_AnyResolvedTarget())
        {
            if (_editorManager.Is_AnimationPreviewActive())
                _editorManager.Stop_AnimationPreview();
        }
        else if (!_editorManager.Is_AnimationPreviewPlaying())
        {
            Refresh_SequenceStaticPreview();
        }
    }

    void UI_Animation_View::Render_SequenceToolbar()
    {
        if (ImGui::Button("Add Json..."))
        {
            _editorManager.Stop_AnimationPreview();
            if (_sequenceService.Add_ClipsFromDialog(_editorManager.Get_Document()))
                Request_SequenceStaticPreviewRefresh();
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(_sequenceService.Is_Empty());
        if (ImGui::Button("Clear"))
        {
            _editorManager.Stop_AnimationPreview();
            _sequenceService.Clear();
            Mark_SequenceStaticPreviewDirty();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        const bool hasSelectedClip = _sequenceService.Get_SelectedClipIndex() >= 0;
        ImGui::BeginDisabled(!hasSelectedClip);
        if (ImGui::Button("Remove"))
        {
            _editorManager.Stop_AnimationPreview();
            if (_sequenceService.Remove_SelectedClip())
                Request_SequenceStaticPreviewRefresh();
        }
        ImGui::SameLine();
        if (ImGui::Button("Move Up"))
        {
            _editorManager.Stop_AnimationPreview();
            if (_sequenceService.Move_SelectedClip(-1))
                Request_SequenceStaticPreviewRefresh();
        }
        ImGui::SameLine();
        if (ImGui::Button("Move Down"))
        {
            _editorManager.Stop_AnimationPreview();
            if (_sequenceService.Move_SelectedClip(1))
                Request_SequenceStaticPreviewRefresh();
        }
        ImGui::EndDisabled();

        const bool canPreview = !_sequenceService.Is_Empty() && _sequenceService.Has_AnyResolvedTarget();
        ImGui::BeginDisabled(!canPreview);
        if (ImGui::Button("Play"))
            _editorManager.Play_AnimationSequencePreview(_sequenceService.Get_Sequence(), _sequenceService.Get_CurrentFrame());
        ImGui::SameLine();
        if (ImGui::Button("Pause"))
        {
            if (_editorManager.Is_AnimationPreviewActive())
                _sequenceService.Set_CurrentFrame(_editorManager.Get_AnimationPreviewFrame());
            _editorManager.Pause_AnimationPreview();
            Request_SequenceStaticPreviewRefresh();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            _editorManager.Stop_AnimationPreview();
            _sequenceService.Set_CurrentFrame(0);
            Request_SequenceStaticPreviewRefresh();
        }
        ImGui::EndDisabled();

        int32 sequenceFps = _sequenceService.Get_Sequence().fps;
        ImGui::SetNextItemWidth(78.f);
        if (ImGui::InputInt("Sequence FPS", &sequenceFps))
        {
            _editorManager.Stop_AnimationPreview();
            _sequenceService.Set_Fps(sequenceFps);
            Request_SequenceStaticPreviewRefresh();
        }

        ImGui::SameLine();
        int32 currentFrame = _sequenceService.Get_CurrentFrame();
        ImGui::SetNextItemWidth(90.f);
        if (ImGui::InputInt("Current Frame", &currentFrame))
        {
            _sequenceService.Set_CurrentFrame(currentFrame);
            Request_SequenceStaticPreviewRefresh();
        }

        ImGui::SameLine();
        ImGui::Text("Total: %d", _sequenceService.Get_TotalFrames());

        if (!_sequenceService.Is_Empty() && !_sequenceService.Has_AnyResolvedTarget())
            ImGui::TextDisabled("No sequence targets are bound in the current UI document.");
    }

    void UI_Animation_View::Render_SequenceClipList()
    {
        const UI_AnimationSequenceAsset& sequence = _sequenceService.Get_Sequence();
        if (sequence.clips.empty())
        {
            ImGui::TextDisabled("Add .uianim.json files to preview them as timeline blocks.");
            return;
        }

        if (ImGui::BeginTable("AnimationSequenceClipList", 6, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, 52.f);
            ImGui::TableSetupColumn("Clip", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("FPS", ImGuiTableColumnFlags_WidthFixed, 48.f);
            ImGui::TableSetupColumn("Frames", ImGuiTableColumnFlags_WidthFixed, 72.f);
            ImGui::TableSetupColumn("Targets", ImGuiTableColumnFlags_WidthFixed, 72.f);

            for (int32 clipIndex = 0; clipIndex < static_cast<int32>(sequence.clips.size()); ++clipIndex)
            {
                const UI_AnimationSequenceClip& clip = sequence.clips[clipIndex];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", clipIndex + 1);

                ImGui::TableSetColumnIndex(1);
                const string selectableLabel = clip.displayName + "##SequenceClip" + to_string(clipIndex);
                if (ImGui::Selectable(
                    selectableLabel.c_str(),
                    _sequenceService.Get_SelectedClipIndex() == clipIndex,
                    ImGuiSelectableFlags_SpanAllColumns))
                {
                    _sequenceService.Select_Clip(clipIndex);
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(Make_ClipFileName(clip).c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", clip.asset.fps);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%d -> %d", clip.asset.startFrame, clip.asset.endFrame);
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%d/%zu", clip.resolvedTargetCount, clip.asset.targets.size());
            }

            ImGui::EndTable();
        }
    }

    void UI_Animation_View::Render_SequenceTimelineSection()
    {
        const ImVec2 timelineSize = ImVec2(ImGui::GetContentRegionAvail().x, max(140.f, ImGui::GetContentRegionAvail().y));
        const UI_AnimationSequenceTimelineClickResult clickResult = _sequenceTimelineView.Render(
            _sequenceService.Get_Sequence(),
            _sequenceService.Get_CurrentFrame(),
            _sequenceService.Get_SelectedClipIndex(),
            timelineSize);
        Handle_SequenceTimelineClick(clickResult);
    }
}
