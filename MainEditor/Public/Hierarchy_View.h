#pragma once

#include "Editor_Window.h"
#include "GameplayUI_Types.h"

NS_BEGIN(MainEditor)

class Hierarchy_View final : public Editor_Window
{
public:
    Hierarchy_View();
    virtual ~Hierarchy_View() = default;

public:
    HRESULT Initialize() override;
    void    Update(float timeDelta) override;
    void    Pre_Render() override;
    void    Render() override;

private:
    void    Draw_SearchBar();
    void    Rebuild_Cache();
    void    Handle_Shortcuts();
    void    Draw_RuntimeUINode();
    void    Draw_RuntimeUIGroup(const char* groupLabel, const vector<Client::RuntimeUIEntry>& entries);
    void    Sync_RuntimeUISelection(const vector<Client::RuntimeUIEntry>& worldEntries, const vector<Client::RuntimeUIEntry>& battleEntries);
    bool    Pass_SearchFilter(const Shared<GameObject>& gameObject, uint32 objectIndex) const;
    bool    Pass_LightingSearchFilter() const;
    bool    Pass_SkyBoxRootSearchFilter() const;
    bool    Is_SkyBoxObject(const Shared<GameObject>& gameObject) const;
    uint32  Get_TotalObjectCount() const;

    void    Draw_LightingNode();
    void    Draw_SkyBoxNode();

    // PartObject 하이어라이키 노출을 위한 함수
    void    Draw_NodeObject(const Shared<GameObject>& gameObject, uint32 objectIndex);
    // Get_Name은 클래스 이름만 가져와져서 Label 세팅해서 보여주기
    string  Build_HierarchyLabel(const Shared<GameObject>& gameObject , uint32 objectIndex) const;

private:
    map<wstring, vector<Shared<GameObject>>> _groupedObjects = {};
    vector<Shared<GameObject>> _skyBoxObjects = {};
    char    _searchBuffer[128] = {};
    string  _searchFilter = {};
    uint32  _cachedSelectionRevision = 0;
    uint32  _cachedLevelIndex = UINT32_MAX;
    uint64  _cachedObjectRevision = UINT64_MAX;
    bool    _dirtyHierarchyCache = true;
    bool    _isPrimaryShadowLightSelected = false;
    bool    _scrollSelectionIntoView = false;

public:
    static Shared<Hierarchy_View> Create();
    void   Free() override;
};

NS_END
