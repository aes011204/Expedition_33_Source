#include "pch.h"
#include "PanelHierarchy.h"
#include "SceneContext.h"

PanelHierarchy::PanelHierarchy(SceneContext* ctx)
    : MEWindow{ L"Hierarchy" }, _ctx{ ctx }
{
    for (int i = 0; i < static_cast<int>(EObjectType::Fail); ++i)
    {
        _types[i] = static_cast<EObjectType>(i);
    }
}

void PanelHierarchy::Render()
{
    bool open = IsOpen();
    if (ImGui::Begin("Hierarchy", &open))
    {
        if (!_ctx)
        {
            ImGui::TextDisabled("(씬 없음)");
            ImGui::End();
            SetOpen(open);
            return;
        }

        if (ImGui::Button("+ 추가"))
        {
            ImGui::OpenPopup("##add_type");
        }

        if (ImGui::BeginPopup("##add_type"))
        {
            for (auto t : _types)
            {
                if (ImGui::MenuItem(ObjectTypeName(t)))
                {
                    wstring name = String::ToWString((ObjectTypeName(t)))
                        + L"_" + to_wstring(_ctx->_objects.size());
                    auto obj = EditorObject::Create(name, t);
                    _ctx->Add(obj);
                    _ctx->_selected = obj;
                }
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("(%d)", static_cast<int>(_ctx->_objects.size()));

        string folderStr = String::ToString(_ctx->_modelFolder);
        strncpy_s(_modelFolderBuf, folderStr.c_str(), sizeof(_modelFolderBuf) - 1);
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::InputText("ModelFolder", _modelFolderBuf, sizeof(_modelFolderBuf)))
        {
            _ctx->_modelFolder = String::ToWString(string(_modelFolderBuf));
        }

        ImGui::Separator();

        if (_ctx->_selected != _prevSelected)
        {
            _prevSelected = _ctx->_selected;
            _needScrollToSelected = true;
        }

        ImGui::BeginChild("##hier_list", ImVec2(0.f, 0.f), false);

        Shared<EditorObject> toDelete;
        Shared<EditorObject> toDuplicate;

        for (int i = 0; i < static_cast<int>(_ctx->_objects.size()); ++i)
        {
            auto& obj = _ctx->_objects[i];
            if (!obj)
            {
                continue;
            }

            ImGui::PushID(i);

            if (_renameIndex == i)
            {
                if (_justStartedRename)
                {
                    ImGui::SetKeyboardFocusHere();
                    _justStartedRename = false;
                }
                ImGui::SetNextItemWidth(-1.f);
                const bool done = ImGui::InputText("##rename", _renameBuffer, sizeof(_renameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

                if (done || ImGui::IsItemDeactivated())
                {
                    obj->_name = String::ToWString(string(_renameBuffer));
                    _renameIndex = -1;
                }
            }
            else
            {
                const bool isSelected = (_ctx->_selected == obj);
                const string nameStr = string(ObjectTypeLabel(obj->_type)) + " " + String::ToString(obj->_name);

                if (ImGui::Selectable(nameStr.c_str(), isSelected, ImGuiSelectableFlags_AllowOverlap))
                {
                    _ctx->_selected = obj;
                }

                if (isSelected && _needScrollToSelected)
                {
                    ImGui::SetScrollHereY(0.5f);
                    _needScrollToSelected = false;
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    StartRename(i, obj);
                }

                if (ImGui::BeginPopupContextItem("##ctx"))
                {
                    _ctx->_selected = obj;
                    if (ImGui::MenuItem("이름 변경"))
                    {
                        StartRename(i, obj);
                    }

                    if (ImGui::MenuItem("복제"))
                    {
                        toDuplicate = obj;
                    }

                    ImGui::Separator();
                    if (ImGui::MenuItem("삭제"))
                    {
                        toDelete = obj;
                    }

                    ImGui::EndPopup();
                }
            }
            ImGui::PopID();
        }

        if (_ctx->_objects.empty())
        {
            ImGui::TextDisabled("(오브젝트 없음)");
        }
        ImGui::EndChild();

        if (toDuplicate)
        {
            _ctx->_selected = _ctx->Duplicate(toDuplicate);
        }

        if (toDelete)
        {
            _ctx->Remove(toDelete);
        }
    }
    ImGui::End();
    SetOpen(open);
}

void PanelHierarchy::StartRename(int index, const Shared<EditorObject>& obj)
{
    _renameIndex = index;
    const string currentName = String::ToString(obj->_name);
    strncpy_s(_renameBuffer, currentName.c_str(), sizeof(_renameBuffer) - 1);
    _justStartedRename = true;
}

Shared<PanelHierarchy> PanelHierarchy::Create(SceneContext* ctx)
{
    return make_shared<PanelHierarchy>(ctx);
}
