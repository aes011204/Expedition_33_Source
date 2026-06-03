#include "pch.h"
#include "UI_Animation_View.h"

#include "UI_Editor_Manager.h"

namespace UIEditor
{
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

        if (_editorMode == UI_AnimationEditorMode::SequencePreview)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
            {
                _editorManager.Stop_AnimationPreview();
                if (_sequenceService.Remove_SelectedClip())
                    Request_SequenceStaticPreviewRefresh();
            }
            return;
        }

        if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            if (_editorMode == UI_AnimationEditorMode::StateAnimation)
                Redo_StateAnimationEdit();
            else
                Redo_KeyframeAnimationEdit();
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
        {
            if (_editorMode == UI_AnimationEditorMode::StateAnimation)
                Redo_StateAnimationEdit();
            else
                Redo_KeyframeAnimationEdit();
            return;
        }

        if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            if (_editorMode == UI_AnimationEditorMode::StateAnimation)
                Undo_StateAnimationEdit();
            else
                Undo_KeyframeAnimationEdit();
            return;
        }

        if (_editorMode == UI_AnimationEditorMode::StateAnimation)
            return;

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
        {
            _assetService.Copy_SelectedKey();
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
        {
            if (_assetService.Paste_CopiedKey())
                Request_StaticPreviewRefresh();
            return;
        }

        if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_K, false))
        {
            if (_assetService.Add_KeyToSelectedTrack(_editorManager.Get_Document()))
                Request_StaticPreviewRefresh();
            return;
        }

        if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_B, false))
        {
            if (_assetService.Bind_SelectedUI(_editorManager.Get_SelectedObject()))
            {
                Sync_TargetNameBuffer();
                Request_StaticPreviewRefresh();
            }
            return;
        }

        if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_R, false))
        {
            if (_assetService.Remove_SelectedTarget())
            {
                Sync_TargetNameBuffer();
                _editorManager.Stop_AnimationPreview();
                Request_StaticPreviewRefresh();
            }
            return;
        }

        if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_T, false))
        {
            if (_assetService.Add_Track(_assetService.Get_PendingTrackType(), &_editorManager.Get_Document()))
                Request_StaticPreviewRefresh();
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
        {
            Delete_SelectedAnimationItem();
            return;
        }
    }

    bool UI_Animation_View::Undo_KeyframeAnimationEdit()
    {
        if (!_assetService.Undo_AssetEdit())
            return false;

        _editorManager.Stop_AnimationPreview();
        Sync_AssetNameBuffer();
        Sync_TargetNameBuffer();
        Request_StaticPreviewRefresh();
        return true;
    }

    bool UI_Animation_View::Redo_KeyframeAnimationEdit()
    {
        if (!_assetService.Redo_AssetEdit())
            return false;

        _editorManager.Stop_AnimationPreview();
        Sync_AssetNameBuffer();
        Sync_TargetNameBuffer();
        Request_StaticPreviewRefresh();
        return true;
    }

    bool UI_Animation_View::Delete_SelectedAnimationItem()
    {
        if (nullptr != _assetService.Get_SelectedKey())
        {
            if (_assetService.Delete_SelectedKey())
            {
                Request_StaticPreviewRefresh();
                return true;
            }
        }

        if (nullptr != _assetService.Get_SelectedTrack())
        {
            if (_assetService.Delete_SelectedTrack())
            {
                Request_StaticPreviewRefresh();
                return true;
            }
        }

        return false;
    }

    bool UI_Animation_View::Undo_StateAnimationEdit()
    {
        if (!_stateAssetService.Undo_AssetEdit())
            return false;

        Sync_StateAssetNameBuffer();
        Sync_StateEventPathBuffers();
        return true;
    }

    bool UI_Animation_View::Redo_StateAnimationEdit()
    {
        if (!_stateAssetService.Redo_AssetEdit())
            return false;

        Sync_StateAssetNameBuffer();
        Sync_StateEventPathBuffers();
        return true;
    }
}
