#include "pch.h"
#include "PanelHierarchy.h"
#include "SceneContext.h"
#include "CmdDeleteObject.h"

namespace
{
    string ToLowerAscii(string value)
    {
        transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(tolower(c));
            });
        return value;
    }

    bool ContainsSearchText(const string& value, const string& searchText)
    {
        return ToLowerAscii(value).find(searchText) != string::npos;
    }
}

PanelHierarchy::PanelHierarchy(SceneContext* ctx)
    : MEWindow{ L"Hierarchy" }, _ctx{ ctx }
{
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

        ImGui::TextDisabled("(%d)", static_cast<int>(_ctx->_objects.size()));

        string folderStr = String::ToString(_ctx->_modelFolder);
        strncpy_s(_modelFolderBuf, folderStr.c_str(), sizeof(_modelFolderBuf) - 1);
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::InputText("ModelFolder", _modelFolderBuf, sizeof(_modelFolderBuf)))
        {
            _ctx->_modelFolder = String::ToWString(string(_modelFolderBuf));
        }

        const bool hasSearchText = _searchBuffer[0] != '\0';
        const float clearButtonWidth = ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2.f;
        const float searchWidth = hasSearchText
            ? max(80.f, ImGui::GetContentRegionAvail().x - clearButtonWidth - ImGui::GetStyle().ItemSpacing.x)
            : -1.f;
        ImGui::SetNextItemWidth(searchWidth);
        ImGui::InputTextWithHint("##SearchHierarchy", "Search...", _searchBuffer, sizeof(_searchBuffer));
        if (hasSearchText)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear"))
            {
                _searchBuffer[0] = '\0';
            }
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
        int visibleCount = 0;

        for (int i = 0; i < static_cast<int>(_ctx->_objects.size()); ++i)
        {
            auto& obj = _ctx->_objects[i];
            if (!obj)
            {
                continue;
            }

            if (!PassSearchFilter(obj))
            {
                continue;
            }
            ++visibleCount;

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

                    if (!obj->_isDefaultLight)
                    {
                        if (ImGui::MenuItem("복제"))
                        {
                            toDuplicate = obj;
                        }

                        ImGui::Separator();
                        if (ImGui::MenuItem("삭제"))
                        {
                            toDelete = obj;
                        }
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
        else if (visibleCount == 0)
        {
            ImGui::TextDisabled("(검색 결과 없음)");
        }
        ImGui::EndChild();

        if (toDuplicate)
        {
            _ctx->_selected = _ctx->Duplicate(toDuplicate);
        }

        if (toDelete)
        {
            auto it = find(_ctx->_objects.begin(), _ctx->_objects.end(), toDelete);
            if (it == _ctx->_objects.end())
            {
                ImGui::End();
                SetOpen(open);
                return;
            }
            
            int index = static_cast<int>(distance(_ctx->_objects.begin(), it));
            _ctx->PushCommand(CmdDeleteObject::Create(toDelete, index));
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

bool PanelHierarchy::PassSearchFilter(const Shared<EditorObject>& obj) const
{
    if (!obj || _searchBuffer[0] == '\0')
    {
        return true;
    }

    const string searchText = ToLowerAscii(_searchBuffer);
    const string name = String::ToString(obj->_name);
    if (ContainsSearchText(name, searchText) ||
        ContainsSearchText(ObjectTypeLabel(obj->_type), searchText) ||
        ContainsSearchText(ObjectTypeName(obj->_type), searchText))
    {
        return true;
    }

    if (!obj->_modelPath.empty())
    {
        const string modelPath = String::ToString(obj->_modelPath);
        if (ContainsSearchText(modelPath, searchText) ||
            ContainsSearchText(fs::path(modelPath).filename().string(), searchText))
        {
            return true;
        }
    }

    if (!obj->_instancedModelPath.empty())
    {
        const string modelPath = String::ToString(obj->_instancedModelPath);
        if (ContainsSearchText(modelPath, searchText) ||
            ContainsSearchText(fs::path(modelPath).filename().string(), searchText))
        {
            return true;
        }
    }

    return false;
}

Shared<PanelHierarchy> PanelHierarchy::Create(SceneContext* ctx)
{
    return make_shared<PanelHierarchy>(ctx);
}
