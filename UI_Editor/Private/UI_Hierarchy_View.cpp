#include "pch.h"
#include "UI_Hierarchy_View.h"
#include "Debug_Profile.h"
#include "UI_Editor_Manager.h"

namespace UIEditor
{
    namespace
    {
        constexpr const char* kHierarchyDragDropPayload = "UI_EDITOR_HIERARCHY_OBJECT";
    }

    UI_Hierarchy_View::UI_Hierarchy_View(UI_Editor_Manager& editorManager)
        : UI_Editor_Window{ L"Hierarchy" }
        , _editorManager{ editorManager }
    {
    }

    void UI_Hierarchy_View::Render()
    {
        if (!Is_Open())
            return;

        ENGINE_PROFILE_SCOPE_BASIC("UIEditor.Hierarchy.Render");

        bool isOpen = Is_Open();
        const string windowName = String::ToString(Get_WindowName());

        if (ImGui::Begin(windowName.c_str(), &isOpen))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

            ImGuiTreeNodeFlags canvasFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (!_editorManager.Has_SelectedObjects())
                canvasFlags |= ImGuiTreeNodeFlags_Selected;

            const string& canvasName = _editorManager.Get_Document().Get_CanvasName();
            const char* canvasLabel = canvasName.empty() ? "Canvas" : canvasName.c_str();
            const bool isCanvasOpen = ImGui::TreeNodeEx("CanvasRoot", canvasFlags, "%s", canvasLabel);
            if (ImGui::IsItemClicked())
                _editorManager.Clear_Selection();

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kHierarchyDragDropPayload))
                {
                    const ObjectId droppedObjectId = *static_cast<const ObjectId*>(payload->Data);
                    _editorManager.Reparent_UIObject(droppedObjectId, 0);
                }

                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginPopupContextItem("CanvasContextMenu", ImGuiPopupFlags_MouseButtonRight))
            {
                _editorManager.Clear_Selection();
                Render_ContextMenu(0);
                ImGui::EndPopup();
            }

            if (isCanvasOpen)
            {
                Expand_SelectedParentPath();
                Render_HierarchyRows();
                ImGui::TreePop();
            }

            if (ImGui::BeginPopupContextWindow("HierarchyEmptyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                _editorManager.Clear_Selection();
                Render_ContextMenu(0);
                ImGui::EndPopup();
            }
        }

        Set_Open(isOpen);
        ImGui::End();

        Render_RenamePopup();
        Apply_PendingActions();
    }

    void UI_Hierarchy_View::Expand_SelectedParentPath()
    {
        const auto& readModel = _editorManager.Get_ReadModel();
        const UI_Object* object = readModel.Find_Object(_editorManager.Get_SelectedObjectId());
        unordered_set<ObjectId> visitedObjectIds;

        while (nullptr != object && object->parentId != 0)
        {
            if (!visitedObjectIds.insert(object->id).second)
                break;

            if (_expandedObjectIds.insert(object->parentId).second)
                ++_hierarchyRowsRevision;

            object = readModel.Find_Object(object->parentId);
        }
    }

    void UI_Hierarchy_View::Render_HierarchyRows()
    {
        const auto& readModel = _editorManager.Get_ReadModel();
        readModel.Rebuild_HierarchyRowsIfNeeded(_expandedObjectIds, _hierarchyRowsRevision);
        const vector<UI_HierarchyRow>& rows = readModel.Get_HierarchyRows();
        const uint64 documentChangeSerial = _editorManager.Get_DocumentChangeSerial();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int32>(rows.size()));
        while (clipper.Step())
        {
            bool stopRenderingRows = false;
            const int32 rowCount = static_cast<int32>(rows.size());
            const int32 displayStart = max(0, min(clipper.DisplayStart, rowCount));
            const int32 displayEnd = max(displayStart, min(clipper.DisplayEnd, rowCount));
            for (int32 rowIndex = displayStart; rowIndex < displayEnd; ++rowIndex)
            {
                const UI_HierarchyRow row = rows[static_cast<size_t>(rowIndex)];
                Render_ObjectRow(row);
                if (_editorManager.Get_DocumentChangeSerial() != documentChangeSerial)
                {
                    stopRenderingRows = true;
                    break;
                }
            }

            if (stopRenderingRows)
            {
                clipper.End();
                break;
            }
        }
    }

    void UI_Hierarchy_View::Render_ObjectRow(const UI_HierarchyRow& row)
    {
        const auto& readModel = _editorManager.Get_ReadModel();
        if (nullptr == row.object)
            return;

        const UI_Object& object = *row.object;
        const bool isEffectivelyVisible = readModel.Is_EffectivelyVisible(object.id);
        const bool isEffectivelyLocked = readModel.Is_EffectivelyLocked(object.id);

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (!row.hasChildren)
            nodeFlags |= ImGuiTreeNodeFlags_Leaf;
        if (_editorManager.Is_ObjectSelected(object.id))
            nodeFlags |= ImGuiTreeNodeFlags_Selected;

        ImGui::PushID(static_cast<int32>(object.id));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, isEffectivelyVisible ? 1.f : 0.45f);
        if (row.depth > 0)
            ImGui::Indent(static_cast<float>(row.depth) * 18.f);

        const char* visibilityLabel = object.visible ? "V" : "-";
        if (ImGui::SmallButton(visibilityLabel))
            _editorManager.Set_ObjectVisible(object.id, !object.visible);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(object.visible ? "Hide in editor viewport" : "Show in editor viewport");

        ImGui::SameLine();
        const char* lockLabel = object.locked ? "L" : "U";
        if (ImGui::SmallButton(lockLabel))
            _editorManager.Set_ObjectLocked(object.id, !object.locked);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(object.locked ? "Unlock editing" : "Lock editing");

        ImGui::SameLine();
        if (row.hasChildren)
            ImGui::SetNextItemOpen(row.isExpanded, ImGuiCond_Always);
        if (isEffectivelyLocked)
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 217, 102, 255));

        const bool isOpen = ImGui::TreeNodeEx("ObjectNode", nodeFlags, "%s", object.name.c_str());
        if (isEffectivelyLocked)
            ImGui::PopStyleColor();

        if (row.hasChildren && isOpen != row.isExpanded)
        {
            if (isOpen)
                _expandedObjectIds.insert(object.id);
            else
                _expandedObjectIds.erase(object.id);
            ++_hierarchyRowsRevision;
        }

        if (ImGui::IsItemClicked())
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl)
                _editorManager.Toggle_Selection(object.id);
            else if (io.KeyShift)
                _editorManager.Add_ToSelection(object.id);
            else
                _editorManager.Set_SingleSelection(object.id);
        }

        if (!isEffectivelyLocked && ImGui::BeginDragDropSource())
        {
            const ObjectId payloadObjectId = object.id;
            ImGui::SetDragDropPayload(kHierarchyDragDropPayload, &payloadObjectId, sizeof(payloadObjectId));
            ImGui::TextUnformatted(object.name.c_str());
            ImGui::EndDragDropSource();
        }

        if ((object.type == UI_ObjectType::Panel || object.type == UI_ObjectType::Button) && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kHierarchyDragDropPayload))
            {
                const ObjectId droppedObjectId = *static_cast<const ObjectId*>(payload->Data);
                _editorManager.Reparent_UIObject(droppedObjectId, object.id);
            }

            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem())
        {
            if (!_editorManager.Is_ObjectSelected(object.id))
                _editorManager.Set_SingleSelection(object.id);
            Render_ContextMenu(object.id);
            ImGui::EndPopup();
        }

        if (row.depth > 0)
            ImGui::Unindent(static_cast<float>(row.depth) * 18.f);
        ImGui::PopStyleVar();
        ImGui::PopID();
    }

    void UI_Hierarchy_View::Render_ContextMenu(ObjectId objectId)
    {
        Render_CreateMenu();

        if (objectId == 0)
        {
            ImGui::Separator();

            if (ImGui::MenuItem("Rename"))
                _editorManager.Request_RenameCanvas();

            return;
        }

        ImGui::Separator();

        const bool isLocked = _editorManager.Is_ObjectEffectivelyLocked(objectId);

        if (ImGui::MenuItem("Rename", nullptr, false, !isLocked))
            Begin_RenameObject(objectId);

        if (ImGui::MenuItem("Duplicate", nullptr, false, !isLocked))
            _pendingDuplicateObjectId = objectId;

        if (ImGui::MenuItem("Delete", nullptr, false, !isLocked))
            _pendingDeleteObjectId = objectId;
    }

    void UI_Hierarchy_View::Render_CreateMenu()
    {
        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Image"))
            {
                _pendingCreateObjectType = UI_ObjectType::Image;
                _hasPendingCreateObject = true;
            }

            if (ImGui::MenuItem("Text"))
            {
                _pendingCreateObjectType = UI_ObjectType::Text;
                _hasPendingCreateObject = true;
            }

            if (ImGui::MenuItem("Panel"))
            {
                _pendingCreateObjectType = UI_ObjectType::Panel;
                _hasPendingCreateObject = true;
            }

            if (ImGui::MenuItem("Button"))
            {
                _pendingCreateObjectType = UI_ObjectType::Button;
                _hasPendingCreateObject = true;
            }

            if (ImGui::MenuItem("UI VFX"))
            {
                _pendingCreateObjectType = UI_ObjectType::UIVfx;
                _hasPendingCreateObject = true;
            }

            ImGui::EndMenu();
        }
    }

    void UI_Hierarchy_View::Render_RenamePopup()
    {
        if (_openRenamePopup)
        {
            ImGui::OpenPopup("Rename UI Object");
            _openRenamePopup = false;
        }

        if (ImGui::BeginPopupModal("Rename UI Object", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            const UI_Object* object = _editorManager.Get_Document().Find_Object(_renamingObjectId);
            if (nullptr == object)
            {
                ImGui::CloseCurrentPopup();
                _renamingObjectId = 0;
                ImGui::EndPopup();
                return;
            }

            ImGui::Text("ID: %u", object->id);
            ImGui::SetNextItemWidth(280.f);
            const bool submitted = ImGui::InputText("Name", _renameBuffer, size(_renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

            if (submitted || ImGui::Button("OK"))
            {
                if (_renameBuffer[0] != '\0')
                    _editorManager.Rename_UIObject(_renamingObjectId, _renameBuffer);

                _renamingObjectId = 0;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                _renamingObjectId = 0;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void UI_Hierarchy_View::Apply_PendingActions()
    {
        if (_pendingDeleteObjectId != 0)
        {
            if (_editorManager.Is_ObjectSelected(_pendingDeleteObjectId) && _editorManager.Get_SelectedObjectCount() > 1)
                _editorManager.Delete_SelectedUIObject();
            else
                _editorManager.Delete_UIObject(_pendingDeleteObjectId);
            _pendingDeleteObjectId = 0;
            _pendingDuplicateObjectId = 0;
            _hasPendingCreateObject = false;
            return;
        }

        if (_pendingDuplicateObjectId != 0)
        {
            if (_editorManager.Is_ObjectSelected(_pendingDuplicateObjectId) && _editorManager.Get_SelectedObjectCount() > 1)
                _editorManager.Duplicate_SelectedUIObject();
            else
                _editorManager.Duplicate_UIObject(_pendingDuplicateObjectId);
            _pendingDuplicateObjectId = 0;
            _hasPendingCreateObject = false;
            return;
        }

        if (_hasPendingCreateObject)
        {
            _editorManager.Create_UIObject(_pendingCreateObjectType);
            _hasPendingCreateObject = false;
        }
    }

    void UI_Hierarchy_View::Begin_RenameObject(ObjectId objectId)
    {
        const UI_Object* object = _editorManager.Get_Document().Find_Object(objectId);
        if (nullptr == object || _editorManager.Is_ObjectEffectivelyLocked(objectId))
            return;

        _renamingObjectId = objectId;
        strncpy_s(_renameBuffer, object->name.c_str(), _TRUNCATE);
        _openRenamePopup = true;
    }

    Shared<UI_Hierarchy_View> UI_Hierarchy_View::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Hierarchy_View>(editorManager);
    }
}
