#include "pch.h"
#include "UI_Animation_View.h"

namespace UIEditor
{
    void UI_Animation_View::Render_StateAnimationEditor()
    {
        Render_StateAnimationToolbar();
        ImGui::Separator();
        Render_StateAnimationEvents();
    }

    void UI_Animation_View::Render_StateAnimationToolbar()
    {
        if (ImGui::Button("New") && _stateAssetService.Confirm_DiscardDirtyAsset(L"create a new state animation asset"))
        {
            _stateAssetService.New_Asset();
            Sync_StateAssetNameBuffer();
            Sync_StateEventPathBuffers();
        }

        ImGui::SameLine();
        if (ImGui::Button("Load") && _stateAssetService.Confirm_DiscardDirtyAsset(L"load another state animation asset"))
        {
            if (_stateAssetService.Load_Asset())
            {
                Sync_StateAssetNameBuffer();
                Sync_StateEventPathBuffers();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Save"))
            _stateAssetService.Save_Asset(false);

        ImGui::SameLine();
        if (ImGui::Button("Save As"))
            _stateAssetService.Save_Asset(true);

        ImGui::SameLine();
        ImGui::BeginDisabled(!_stateAssetService.Can_UndoAssetEdit());
        if (ImGui::Button("Undo"))
            Undo_StateAnimationEdit();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!_stateAssetService.Can_RedoAssetEdit());
        if (ImGui::Button("Redo"))
            Redo_StateAnimationEdit();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.f);
        if (ImGui::InputText("State Asset Name", _stateAssetNameBuffer, size(_stateAssetNameBuffer)))
            _stateAssetService.Set_AssetName(_stateAssetNameBuffer);

        if (_stateAssetService.Is_Dirty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("*");
        }
    }

    void UI_Animation_View::Render_StateAnimationEvents()
    {
        ImGui::TextDisabled("State events play Public_Animation or Target_Animation assets. Paths are resource-relative and usually omit .uianim.json.");

        if (ImGui::BeginTable("StateAnimationEvents", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthFixed, 130.f);
            ImGui::TableSetupColumn("Animation Path", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 118.f);

            for (int32 index = 0; index < static_cast<int32>(UI_StateAnimationEvent::Count); ++index)
            {
                const UI_StateAnimationEvent eventType = static_cast<UI_StateAnimationEvent>(index);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(Get_StateAnimationEventName(eventType));
                ImGui::TableSetColumnIndex(1);

                ImGui::PushID(index);
                array<char, 260>& pathBuffer = _stateEventPathBuffers[static_cast<size_t>(eventType)];
                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::InputText("##AnimationPath", pathBuffer.data(), pathBuffer.size()))
                    _stateAssetService.Set_EventAnimationPath(eventType, pathBuffer.data());

                ImGui::TableSetColumnIndex(2);
                if (ImGui::Button("Select"))
                {
                    if (_stateAssetService.Select_AnimationForEvent(eventType))
                        Sync_StateEventPathBuffers();
                }

                ImGui::SameLine();
                ImGui::BeginDisabled(pathBuffer[0] == '\0');
                if (ImGui::Button("Clear"))
                {
                    _stateAssetService.Set_EventAnimationPath(eventType, "");
                    pathBuffer[0] = '\0';
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }
}
