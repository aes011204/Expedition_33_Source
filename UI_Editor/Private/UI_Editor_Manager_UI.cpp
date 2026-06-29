#include "pch.h"
#include "UI_Editor_Manager.h"

namespace UIEditor
{
    void UI_Editor_Manager::Render_DockSpace()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (nullptr == viewport)
            return;

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

        bool isOpen = true;
        ImGui::Begin("UIEditorDockSpace", &isOpen, windowFlags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockSpaceId = ImGui::GetID("UIEditorDockSpaceId");
        ImGui::DockSpace(dockSpaceId, ImVec2(0.f, 0.f), ImGuiDockNodeFlags_None);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New"))
                    New_Document();
                if (ImGui::MenuItem("Save"))
                    Save_Document();
                if (ImGui::MenuItem("Save As"))
                    Save_DocumentAs();
                if (ImGui::MenuItem("Load"))
                    Load_Document();

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem("Image"))
                    Create_UIObject(UI_ObjectType::Image);

                if (ImGui::MenuItem("Text"))
                    Create_UIObject(UI_ObjectType::Text);

                if (ImGui::MenuItem("Panel"))
                    Create_UIObject(UI_ObjectType::Panel);

                if (ImGui::MenuItem("Button"))
                    Create_UIObject(UI_ObjectType::Button);

                if (ImGui::MenuItem("UI VFX"))
                    Create_UIObject(UI_ObjectType::UIVfx);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                for (auto& pair : _windows)
                {
                    if (nullptr == pair.second)
                        continue;

                    bool isWindowOpen = pair.second->Is_Open();
                    const string windowName = String::ToString(pair.first);
                    if (ImGui::MenuItem(windowName.c_str(), nullptr, &isWindowOpen))
                        pair.second->Set_Open(isWindowOpen);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, Can_Undo()))
                    Undo();
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, Can_Redo()))
                    Redo();

                ImGui::Separator();
                if (ImGui::MenuItem("Copy", "Ctrl+C", false, Has_SelectedObjects()))
                    Copy_SelectedUIObjects();
                if (ImGui::MenuItem("Paste", "Ctrl+V", false, Has_CopiedUIObjects()))
                    Paste_UIObjects();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, Has_SelectedObjects()))
                    Duplicate_SelectedUIObject();

                ImGui::Separator();
                if (ImGui::BeginMenu("Align", Get_SelectedObjectCount() >= 2))
                {
                    if (ImGui::MenuItem("Left"))
                        Align_SelectedObjects(UI_AlignmentCommand::Left);
                    if (ImGui::MenuItem("Right"))
                        Align_SelectedObjects(UI_AlignmentCommand::Right);
                    if (ImGui::MenuItem("Top"))
                        Align_SelectedObjects(UI_AlignmentCommand::Top);
                    if (ImGui::MenuItem("Bottom"))
                        Align_SelectedObjects(UI_AlignmentCommand::Bottom);
                    if (ImGui::MenuItem("Center X"))
                        Align_SelectedObjects(UI_AlignmentCommand::CenterX);
                    if (ImGui::MenuItem("Center Y"))
                        Align_SelectedObjects(UI_AlignmentCommand::CenterY);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Distribute Horizontal", nullptr, false, Get_SelectedObjectCount() >= 3))
                        Align_SelectedObjects(UI_AlignmentCommand::DistributeHorizontal);
                    if (ImGui::MenuItem("Distribute Vertical", nullptr, false, Get_SelectedObjectCount() >= 3))
                        Align_SelectedObjects(UI_AlignmentCommand::DistributeVertical);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    void UI_Editor_Manager::Handle_Shortcuts()
    {
        const ImGuiIO& io = ImGui::GetIO();
        const bool hasBlockingPopup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);

        if (!io.WantTextInput && !hasBlockingPopup && ImGui::IsKeyPressed(ImGuiKey_F2, false))
        {
            if (Get_SelectedObjectId() == 0)
                Request_RenameCanvas();
            else
                Request_RenameUIObject(Get_SelectedObjectId());
            return;
        }

        if (io.WantTextInput ||
            ImGui::IsAnyItemActive() ||
            hasBlockingPopup)
        {
            return;
        }

        const Shared<UI_Editor_Window> animationWindow = Find_Window(L"Animation");
        if (nullptr != animationWindow && animationWindow->Is_Open() && animationWindow->Is_Focused())
            return;

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            Undo();
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
        {
            Redo();
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
        {
            Copy_SelectedUIObjects();
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
        {
            Paste_UIObjects();
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            Delete_SelectedUIObject();
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
        {
            Duplicate_SelectedUIObject();
            return;
        }
    }

    void UI_Editor_Manager::Render_RenamePopup()
    {
        const char* popupTitle = _isRenamingCanvas ? "Rename Canvas" : "Rename Selected UI Object";

        if (_openRenamePopup)
        {
            ImGui::OpenPopup(popupTitle);
            _openRenamePopup = false;
        }

        if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            const UI_Object* object = _isRenamingCanvas ? nullptr : static_cast<const UI_Document&>(_document).Find_Object(_renameRequestObjectId);
            if (!_isRenamingCanvas && nullptr == object)
            {
                _renameRequestObjectId = 0;
                _isRenamingCanvas = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }

            if (_isRenamingCanvas)
            {
                ImGui::TextUnformatted("Type: Canvas");
            }
            else
            {
                ImGui::Text("ID: %u", object->id);
            }

            ImGui::SetNextItemWidth(280.f);
            const bool submitted = ImGui::InputText("Name", _renameBuffer, size(_renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

            if (submitted || ImGui::Button("OK"))
            {
                if (_isRenamingCanvas)
                    Rename_Canvas(_renameBuffer);
                else
                    Rename_UIObject(_renameRequestObjectId, _renameBuffer);

                _renameRequestObjectId = 0;
                _isRenamingCanvas = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                _renameRequestObjectId = 0;
                _isRenamingCanvas = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}
