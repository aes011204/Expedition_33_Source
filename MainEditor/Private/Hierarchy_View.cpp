#include "Hierarchy_View.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Client_Defines.h"
#include "ContainerObject.h"
#include "Inspector.h"
#include "PartObject.h"
#include "ClientInstance.h"
#include "GameplayUI.h"

namespace
{
    string To_LowerCopy(string value)
    {
        transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
        {
            return static_cast<char>(tolower(ch));
        });
        return value;
    }

    bool Match_WildcardPattern(const string& text, const string& pattern)
    {
        size_t textIndex = 0;
        size_t patternIndex = 0;
        size_t starPatternIndex = string::npos;
        size_t starTextIndex = 0;

        while (textIndex < text.size())
        {
            if (patternIndex < pattern.size() && pattern[patternIndex] == '*')
            {
                starPatternIndex = patternIndex++;
                starTextIndex = textIndex;
                continue;
            }

            if (patternIndex < pattern.size() && pattern[patternIndex] == text[textIndex])
            {
                ++patternIndex;
                ++textIndex;
                continue;
            }

            if (starPatternIndex != string::npos)
            {
                patternIndex = starPatternIndex + 1;
                textIndex = ++starTextIndex;
                continue;
            }

            return false;
        }

        while (patternIndex < pattern.size() && pattern[patternIndex] == '*')
            ++patternIndex;

        return patternIndex == pattern.size();
    }

    bool Match_SearchPattern(const string& text, const string& rawPattern)
    {
        const string textLower = To_LowerCopy(text);
        const string patternLower = To_LowerCopy(rawPattern);

        if (patternLower.find('*') == string::npos)
            return textLower.find(patternLower) != string::npos;

        return Match_WildcardPattern(textLower, patternLower);
    }
}

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
    {
        _cachedSelectionRevision = selectionRevision;
        _scrollSelectionIntoView = true;
    }

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

        const bool skyBoxRootMatches =
            Match_SearchPattern("SkyBox", _searchFilter) ||
            Match_SearchPattern("Sky", _searchFilter);

        uint32 skyBoxObjectIndex = 0;
        for (const auto& gameObject : _skyBoxObjects)
        {
            if (gameObject != nullptr && (skyBoxRootMatches || Pass_SearchFilter(gameObject, skyBoxObjectIndex)))
                ++filteredCount;

            ++skyBoxObjectIndex;
        }

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
    _skyBoxObjects.clear();

    auto appendLevelObjects =
        [this](uint32 levelIndex, const char* levelLabel)
        {
            GAME->Visit_LevelObjects(
                levelIndex,
                [this, levelLabel](const wstring& layerTag, const Shared<GameObject>& gameObject)
                {
                    if (gameObject == nullptr)
                        return;

                    if (Is_SkyBoxObject(gameObject))
                    {
                        _skyBoxObjects.push_back(gameObject);
                        return;
                    }

                    const wstring groupedLayerTag =
                        String::ToWString(format("[{}] {}", levelLabel, String::ToString(layerTag)));

                    _groupedObjects[groupedLayerTag].push_back(gameObject);
                });
        };

    appendLevelObjects(ETOI(LevelType::Static), "Static");

    const uint32 currentLevelIndex = GAME->Current_LevelIndex();
    if (currentLevelIndex != ETOI(LevelType::Static))
        appendLevelObjects(currentLevelIndex, "Current");

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
    if (Match_SearchPattern(label, _searchFilter))
        return true;

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
    if (container == nullptr)
        return false;

    uint32 partIndex = 0;
    for (const auto& [partTag, partObject] : container->Get_PartObjects())
    {
        if (partObject == nullptr)
        {
            ++partIndex;
            continue;
        }

        const string partLabel = Build_PartHierarchyLabel(partTag, partIndex++);
        if (Match_SearchPattern(partLabel, _searchFilter))
            return true;
    }

    return false;
}

bool Hierarchy_View::Pass_LightingSearchFilter() const
{
    if (_searchFilter.empty())
        return true;

    return Match_SearchPattern("Lighting", _searchFilter) ||
        Match_SearchPattern("Light", _searchFilter) ||
        Match_SearchPattern("Directional Light", _searchFilter) ||
        Match_SearchPattern("Primary Shadow Light", _searchFilter) ||
        Match_SearchPattern("Shadow", _searchFilter);
}

bool Hierarchy_View::Pass_SkyBoxRootSearchFilter() const
{
    if (_searchFilter.empty())
        return true;

    if (Match_SearchPattern("SkyBox", _searchFilter) ||
        Match_SearchPattern("Sky", _searchFilter))
        return true;

    uint32 objectIndex = 0;
    for (const auto& gameObject : _skyBoxObjects)
    {
        if (gameObject != nullptr && Pass_SearchFilter(gameObject, objectIndex))
            return true;

        ++objectIndex;
    }

    return false;
}

bool Hierarchy_View::Is_SkyBoxObject(const Shared<GameObject>& gameObject) const
{
    return gameObject != nullptr && gameObject->Get_ObjectType() == ObjectType::Sky;
}

uint32 Hierarchy_View::Get_TotalObjectCount() const
{
    uint32 totalCount = 0;

    for (const auto& gameObject : _skyBoxObjects)
    {
        if (gameObject != nullptr)
            ++totalCount;
    }

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

    const Shared<PartObject> partObject = dynamic_pointer_cast<PartObject>(gameObject);
    if (partObject == nullptr)
    {
        const string& sourcePrefabName = gameObject->Get_SourcePrefabName();
        if (!sourcePrefabName.empty())
            return sourcePrefabName + " [" + to_string(objectIndex) + "]"; // 프리팹으로 스폰된 루트 오브젝트는 authored prefab 이름을 하이어라키 대표 라벨로 쓴다.
    }

    const string objectName = String::ToString(gameObject->Get_Name());

    if (!objectName.empty())
        return objectName + " [" + to_string(objectIndex) + "]";

    return "GameObject [" + to_string(objectIndex) + "]";
}

string Hierarchy_View::Build_PartHierarchyLabel(const wstring& partTag, uint32 partIndex) const
{
    const string partName = String::ToString(partTag);

    if (!partName.empty())
        return partName + " [" + to_string(partIndex) + "]";

    return "PartObject [" + to_string(partIndex) + "]";
}

void Hierarchy_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        const Shared<GameObject> selectedObject = EDITOR->Selected_Object();
        const bool shouldRevealSelection = _scrollSelectionIntoView && selectedObject != nullptr;
        auto containsSelectedObject = [&selectedObject](const Shared<GameObject>& gameObject)
        {
            if (gameObject == nullptr || selectedObject == nullptr)
                return false;

            if (gameObject == selectedObject)
                return true;

            Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
            if (container == nullptr)
                return false;

            for (const auto& [_, partObject] : container->Get_PartObjects())
            {
                if (partObject == selectedObject)
                    return true;
            }

            return false;
        };

        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        Handle_Shortcuts();

        Draw_SearchBar();
        Draw_LightingNode();
        Draw_SkyBoxNode();

        const bool hasHierarchyObjects = !_groupedObjects.empty() || !_skyBoxObjects.empty();
        if (!hasHierarchyObjects)
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
                    const bool layerHasSelection =
                        shouldRevealSelection &&
                        any_of(objects.begin(), objects.end(), containsSelectedObject);

                    if (layerHasSelection)
                        ImGui::SetNextItemOpen(true, ImGuiCond_Always);

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

        Draw_RuntimeUINode();
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Hierarchy_View::Draw_RuntimeUINode()
{
    Shared<GameplayUI> gameplayUI = CLIENT->Get_GameplayUI();
    if (gameplayUI == nullptr)
    {
        if (EDITOR->Selected_UI() != nullptr ||
            !EDITOR->Selected_UIGroup().empty() ||
            !EDITOR->Selected_UIDisplayName().empty())
        {
            EDITOR->Clear_SelectedUI();
        }

        if (EDITOR->Selected_RuntimeWorldUI() != nullptr ||
            !EDITOR->Selected_RuntimeWorldUIGroup().empty() ||
            !EDITOR->Selected_RuntimeWorldUIDisplayName().empty())
        {
            EDITOR->Clear_SelectedRuntimeWorldUI();
        }
        return;
    }

    const vector<Client::RuntimeUIEntry> worldEntries = gameplayUI->Get_WorldRuntimeUIEntries();
    const vector<Client::RuntimeUIEntry> battleEntries = gameplayUI->Get_BattleRuntimeUIEntries();

    Sync_RuntimeUISelection(worldEntries, battleEntries);

    ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (ImGui::TreeNodeEx("GamePlayerUI", rootFlags))
    {
        Draw_RuntimeUIGroup("World", worldEntries);
        Draw_RuntimeUIGroup("Battle", battleEntries);
        ImGui::TreePop();
    }
}

void Hierarchy_View::Draw_RuntimeUIGroup(const char* groupLabel, const vector<Client::RuntimeUIEntry>& entries)
{
    ImGuiTreeNodeFlags groupFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!ImGui::TreeNodeEx(groupLabel, groupFlags))
        return;

    if (entries.empty())
    {
        ImGui::TextDisabled("No runtime UI");
        ImGui::TreePop();
        return;
    }

    const Shared<UI_Base> selectedUI = EDITOR->Selected_UI();
    Client::IRuntimeWorldUI_EditorTarget* selectedRuntimeWorldUI = EDITOR->Selected_RuntimeWorldUI();
    for (const Client::RuntimeUIEntry& entry : entries)
    {
        const bool isWorldPlacement = entry.kind == Client::RuntimeUIEntryKind::WorldPlacement;
        const bool isSelected = isWorldPlacement
            ? (selectedRuntimeWorldUI != nullptr && selectedRuntimeWorldUI == entry.worldPlacementTarget)
            : (selectedUI == entry.ui);
        ImGuiTreeNodeFlags itemFlags =
            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (isSelected)
            itemFlags |= ImGuiTreeNodeFlags_Selected;

        ImGui::PushID(isWorldPlacement ? static_cast<void*>(entry.worldPlacementTarget) : static_cast<void*>(entry.ui.get()));
        ImGui::TreeNodeEx(entry.name.c_str(), itemFlags);
        if (ImGui::IsItemClicked())
        {
            _isPrimaryShadowLightSelected = false;
            if (Shared<Inspector> inspector = dynamic_pointer_cast<Inspector>(EDITOR->Get_Window(L"Inspector")))
                inspector->Set_PrimaryShadowLightTarget(false);

            if (isWorldPlacement)
            {
                if (entry.worldPlacementTarget != nullptr)
                    EDITOR->Select_RuntimeWorldUI(entry.worldPlacementTarget, entry.category, entry.name);
            }
            else if (entry.ui != nullptr)
            {
                EDITOR->Select_UI(entry.ui, entry.category, entry.name, entry.positionDrivenByCode);
            }
        }
        ImGui::PopID();
    }

    ImGui::TreePop();
}

void Hierarchy_View::Sync_RuntimeUISelection(
    const vector<Client::RuntimeUIEntry>& worldEntries,
    const vector<Client::RuntimeUIEntry>& battleEntries)
{
    const Shared<UI_Base> selectedUI = EDITOR->Selected_UI();
    if (selectedUI == nullptr)
    {
        if (!EDITOR->Selected_UIGroup().empty() || !EDITOR->Selected_UIDisplayName().empty())
            EDITOR->Clear_SelectedUI();
    }
    else
    {
        auto containsSelected = [&selectedUI](const vector<Client::RuntimeUIEntry>& entries)
        {
            for (const Client::RuntimeUIEntry& entry : entries)
            {
                if (entry.ui == selectedUI)
                    return true;
            }

            return false;
        };

        if (!containsSelected(worldEntries) && !containsSelected(battleEntries))
            EDITOR->Clear_SelectedUI();
    }

    Client::IRuntimeWorldUI_EditorTarget* selectedRuntimeWorldUI = EDITOR->Selected_RuntimeWorldUI();
    const bool hasRuntimeWorldUISelection = selectedRuntimeWorldUI != nullptr ||
        !EDITOR->Selected_RuntimeWorldUIGroup().empty() ||
        !EDITOR->Selected_RuntimeWorldUIDisplayName().empty();
    if (!hasRuntimeWorldUISelection)
        return;

    auto containsSelectedRuntimeWorldUI =
        [selectedRuntimeWorldUI](const vector<Client::RuntimeUIEntry>& entries)
        {
            for (const Client::RuntimeUIEntry& entry : entries)
            {
                if (entry.kind == Client::RuntimeUIEntryKind::WorldPlacement &&
                    entry.worldPlacementTarget == selectedRuntimeWorldUI)
                {
                    return true;
                }
            }

            return false;
        };

    if (selectedRuntimeWorldUI == nullptr ||
        (!containsSelectedRuntimeWorldUI(worldEntries) &&
            !containsSelectedRuntimeWorldUI(battleEntries)))
    {
        EDITOR->Clear_SelectedRuntimeWorldUI();
    }
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
    bool containsSelectedPart = false;

    if (_scrollSelectionIntoView && isContainer && selectedObject != nullptr && !isSelected)
    {
        for (const auto& [_, partObject] : container->Get_PartObjects())
        {
            if (partObject == selectedObject)
            {
                containsSelectedPart = true;
                break;
            }
        }
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (isSelected)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (!isContainer)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    if (containsSelectedPart)
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);

    if (!_searchFilter.empty() && isContainer)
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);

    bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);

    if (isSelected && _scrollSelectionIntoView)
    {
        ImGui::SetScrollHereY(0.25f);
        _scrollSelectionIntoView = false;
    }

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        _isPrimaryShadowLightSelected = false;
        if (Shared<Inspector> inspector = dynamic_pointer_cast<Inspector>(EDITOR->Get_Window(L"Inspector")))
            inspector->Set_PrimaryShadowLightTarget(false);

        EDITOR->Select_Object(gameObject);
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        _isPrimaryShadowLightSelected = false;
        if (Shared<Inspector> inspector = dynamic_pointer_cast<Inspector>(EDITOR->Get_Window(L"Inspector")))
            inspector->Set_PrimaryShadowLightTarget(false);

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
            const string partLabel = Build_PartHierarchyLabel(partTag, partIndex++);

            // 파츠 오브젝트는 더 이상 자식이 없다고 가정하고 Leaf 노드로 출력
            ImGuiTreeNodeFlags partFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanAvailWidth;
            if (isPartSelected)
                partFlags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(partLabel.c_str(), partFlags);

            if (isPartSelected && _scrollSelectionIntoView)
            {
                ImGui::SetScrollHereY(0.25f);
                _scrollSelectionIntoView = false;
            }

            if (ImGui::IsItemClicked())
            {
                _isPrimaryShadowLightSelected = false;
                if (Shared<Inspector> inspector = dynamic_pointer_cast<Inspector>(EDITOR->Get_Window(L"Inspector")))
                    inspector->Set_PrimaryShadowLightTarget(false);

                EDITOR->Select_Object(partObject);
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                _isPrimaryShadowLightSelected = false;
                if (Shared<Inspector> inspector = dynamic_pointer_cast<Inspector>(EDITOR->Get_Window(L"Inspector")))
                    inspector->Set_PrimaryShadowLightTarget(false);

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

void Hierarchy_View::Draw_LightingNode()
{
    if (!Pass_LightingSearchFilter())
        return;

    ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (ImGui::TreeNodeEx(ICON_FA_SUN " Lighting", rootFlags))
    {
        const bool hasLight = (GAME->Get_LightCount() > 0);

        if (!hasLight)
        {
            ImGui::TextDisabled("No Light");
        }
        else
        {
            const bool isSelected = _isPrimaryShadowLightSelected && EDITOR->Selected_Object() == nullptr;

            ImGuiTreeNodeFlags itemFlags =
                ImGuiTreeNodeFlags_Leaf |
                ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanAvailWidth;

            if (isSelected)
                itemFlags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(ICON_FA_LIGHTBULB " Primary Shadow Light", itemFlags);

            if (ImGui::IsItemClicked())
            {
                EDITOR->Clear_SelectedObject();
                _isPrimaryShadowLightSelected = true;

                if (Shared<Inspector> inspector = dynamic_pointer_cast<Inspector>(EDITOR->Get_Window(L"Inspector")))
                    inspector->Set_PrimaryShadowLightTarget(true);
            }
        }

        ImGui::TreePop();
    }

    ImGui::Separator();
}

void Hierarchy_View::Draw_SkyBoxNode()
{
    if (_skyBoxObjects.empty() || !Pass_SkyBoxRootSearchFilter())
        return;

    ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (ImGui::TreeNodeEx("SkyBox", rootFlags))
    {
        const bool skyBoxRootMatches =
            Match_SearchPattern("SkyBox", _searchFilter) ||
            Match_SearchPattern("Sky", _searchFilter);

        uint32 objectIndex = 0;
        for (const auto& gameObject : _skyBoxObjects)
        {
            if (gameObject == nullptr)
            {
                ++objectIndex;
                continue;
            }

            if (!_searchFilter.empty() &&
                !skyBoxRootMatches &&
                !Pass_SearchFilter(gameObject, objectIndex))
            {
                ++objectIndex;
                continue;
            }

            Draw_NodeObject(gameObject, objectIndex);
            ++objectIndex;
        }

        ImGui::TreePop();
    }

    ImGui::Separator();
}

Shared<Hierarchy_View> Hierarchy_View::Create()
{
    return make_shared<Hierarchy_View>();
}

void Hierarchy_View::Free()
{
    _groupedObjects.clear();
    _skyBoxObjects.clear();
    Editor_Window::Free();
}
