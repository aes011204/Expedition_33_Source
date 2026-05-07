#include "Hierarchy_View.h"

#include "EditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ContainerObject.h"
#include "PartObject.h"

Hierarchy_View::Hierarchy_View()
    : Editor_Window{ L"Hierarchy" }
{
}

HRESULT Hierarchy_View::Initialize()
{
    return Editor_Window::Initialize();
}

void Hierarchy_View::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);

    const uint32 selectionRevision = EDITOR->Selection_Revision();
    if (_cachedSelectionRevision != selectionRevision)
        _cachedSelectionRevision = selectionRevision;

    const uint32 currentLevelIndex = GAME->Current_LevelIndex();
    if (_cachedLevelIndex != currentLevelIndex)
    {
        _cachedLevelIndex = currentLevelIndex;
        _dirtyHierarchyCache = true;
    }

    const uint64 objectRevision = GAME->Get_CurrentLevelObjectRevision();
    if (_cachedObjectRevision != objectRevision)
    {
        _cachedObjectRevision = objectRevision;
        _dirtyHierarchyCache = true;
    }

    if (_dirtyHierarchyCache)
        Rebuild_Cache();
}

void Hierarchy_View::Pre_Render()
{
    Editor_Window::Pre_Render();
}

void Hierarchy_View::Draw_SearchBar()
{
    ImGui::InputTextWithHint("##SearchHierarchy", "Search...", _searchBuffer, IM_ARRAYSIZE(_searchBuffer));
    _searchFilter = _searchBuffer;

    ImGui::Separator();

    const uint32 totalCount = Get_TotalObjectCount();
    uint32 filteredCount = totalCount;

    if (!_searchFilter.empty())
    {
        filteredCount = 0;

        for (const auto& [_, objects] : _groupedObjects)
        {
            uint32 objectIndex = 0;

            for (const auto& gameObject : objects)
            {
                if (gameObject != nullptr && Pass_SearchFilter(gameObject, objectIndex))
                    ++filteredCount;

                ++objectIndex;
            }
        }
    }

    ImGui::Text("Objects: %u / %u", filteredCount, totalCount);
    ImGui::Separator();
}

void Hierarchy_View::Rebuild_Cache()
{
    _groupedObjects.clear();

    GAME->Visit_CurrentLevelObjects(
        [this](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (gameObject == nullptr)
                return;

            _groupedObjects[layerTag].push_back(gameObject);
        });

    _dirtyHierarchyCache = false;
}

void Hierarchy_View::Handle_Shortcuts()
{
    if (!_isFocused)
        return;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        EDITOR->Copy_SelectedObject();
        return;
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        EDITOR->Paste_CopiedObject();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        EDITOR->Delete_SelectedObject();
    }
}

bool Hierarchy_View::Pass_SearchFilter(const Shared<GameObject>& gameObject, uint32 objectIndex) const
{
    if (_searchFilter.empty())
        return true;

    const string label = Build_HierarchyLabel(gameObject, objectIndex);
    return label.find(_searchFilter) != string::npos;
}

uint32 Hierarchy_View::Get_TotalObjectCount() const
{
    uint32 totalCount = 0;

    for (const auto& [_, objects] : _groupedObjects)
    {
        for (const auto& gameObject : objects)
        {
            if (gameObject != nullptr)
                ++totalCount;
        }
    }

    return totalCount;
}

string Hierarchy_View::Build_HierarchyLabel(const Shared<GameObject>& gameObject, uint32 objectIndex) const
{
    if (gameObject == nullptr)
        return "<Null>";

    const string objectName = String::ToString(gameObject->Get_Name());

    if (!objectName.empty())
        return objectName + " [" + to_string(objectIndex) + "]";

    return "GameObject [" + to_string(objectIndex) + "]";
}

void Hierarchy_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        Handle_Shortcuts();

        const Shared<GameObject> selectedObject = EDITOR->Selected_Object();

        Draw_SearchBar();

        if (_groupedObjects.empty())
        {
            ImGui::TextDisabled("현재 레벨에 오브젝트가 없음");
        }
        else
        {
            if (!_searchFilter.empty())
            {
                for (const auto& [_, objects] : _groupedObjects)
                {
                    uint32 objectIndex = 0;

                    for (const auto& gameObject : objects)
                    {
                        if (gameObject == nullptr)
                        {
                            ++objectIndex;
                            continue;
                        }

                        if (!Pass_SearchFilter(gameObject, objectIndex))
                        {
                            ++objectIndex;
                            continue;
                        }

                        Draw_NodeObject(gameObject, objectIndex);
                        
                        ++objectIndex;
                    }
                }
            }
            else
            {
                for (const auto& [layerTag, objects] : _groupedObjects)
                {
                    const string layerName = String::ToString(layerTag);

                    if (ImGui::TreeNodeEx(layerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        uint32 objectIndex = 0;

                        for (const auto& gameObject : objects)
                        {
                            if (gameObject == nullptr)
                            {
                                ++objectIndex;
                                continue;
                            }

                            Draw_NodeObject(gameObject, objectIndex);

                            ++objectIndex;
                        }

                        ImGui::TreePop();
                    }
                }
            }
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Hierarchy_View::Draw_NodeObject(const Shared<GameObject>& gameObject, uint32 objectIndex)
{
    if (gameObject == nullptr) return;

    const Shared<GameObject> selectedObject = EDITOR->Selected_Object();
    const bool isSelected = (selectedObject == gameObject);
    const string label = Build_HierarchyLabel(gameObject, objectIndex);

    ImGui::PushID(gameObject.get());

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
    bool isContainer = (container != nullptr) && !container->Get_PartObjects().empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (isSelected)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (!isContainer)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        EDITOR->Select_Object(gameObject);
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        EDITOR->Select_Object(gameObject);
        EDITOR->Request_FocusObject(gameObject);
        EDITOR->Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Scene);
    }

    if (nodeOpen && isContainer)
    {
        uint32 partIndex = 0;
        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            if (partObject == nullptr) continue;

            ImGui::PushID(partObject.get());

            const bool isPartSelected = (selectedObject == partObject);
            const string partLabel = Build_HierarchyLabel(partObject, partIndex++);

            // 파츠 오브젝트는 더 이상 자식이 없다고 가정하고 Leaf 노드로 출력
            ImGuiTreeNodeFlags partFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanAvailWidth;
            if (isPartSelected)
                partFlags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(partLabel.c_str(), partFlags);

            if (ImGui::IsItemClicked())
            {
                EDITOR->Select_Object(partObject);
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                EDITOR->Select_Object(partObject);
                EDITOR->Request_FocusObject(partObject);
                EDITOR->Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Scene);
            }

            ImGui::PopID();
        }
        ImGui::TreePop(); // 폴더 닫기
    }

    ImGui::PopID();
}

Shared<Hierarchy_View> Hierarchy_View::Create()
{
    return make_shared<Hierarchy_View>();
}

void Hierarchy_View::Free()
{
    _groupedObjects.clear();
    Editor_Window::Free();
}
