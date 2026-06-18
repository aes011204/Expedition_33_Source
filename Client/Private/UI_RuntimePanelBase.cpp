#include "UI_RuntimePanelBase.h"
#include "UI_Base.h"

void UI_RuntimePanelBase::Open()
{
    _isOpen = true;

    if (_root)
        _root->Open();

    Apply_EditorDefaultOffsetToRoot();
}

void UI_RuntimePanelBase::Close()
{
    _isOpen = false;

    if (_root)
        _root->Close();
}

void UI_RuntimePanelBase::Toggle()
{
    if (_isOpen)
        Close();
    else
        Open();
}

void UI_RuntimePanelBase::Append_RuntimeUIEntries(
    vector<RuntimeUIEntry>& outEntries,
    const string& category,
    const string& rootName) const
{
    if (_root == nullptr)
        return;

    RuntimeUIEntry rootEntry{};
    rootEntry.category = category;
    rootEntry.name = rootName;
    rootEntry.ui = _root;
    rootEntry.editorTarget = const_cast<UI_RuntimePanelBase*>(this); // 같은 패널이 소유한 루트/자식들을 하이어라키에서 한 클래스 아래로 묶는다.
    outEntries.push_back(rootEntry);

    Append_RuntimeUIMapEntries(
        outEntries,
        category,
        _uiByName,
        _root,
        const_cast<UI_RuntimePanelBase*>(this));
}

void UI_RuntimePanelBase::Set_EditorDefaultTestOffset(const Vec2& offset)
{
    _editorDefaultTestOffset = offset;

    Apply_EditorDefaultOffsetToRoot();
}

void UI_RuntimePanelBase::Apply_EditorDefaultOffsetToRoot()
{
    if (_editorPositionDrivenByCode)
        return; 

    if (_root == nullptr || _root->Get_Transform() == nullptr)
        return;

    Capture_EditorDesignPositionIfNeeded();

    Vec2 finalPosition = _editorDesignPosition;
    finalPosition += _editorDefaultTestOffset; 
    _root->Get_Transform()->Set_Position(finalPosition);
}

void UI_RuntimePanelBase::Capture_EditorDesignPositionIfNeeded()
{
    if (_hasEditorDesignPosition)
        return;

    if (_root == nullptr || _root->Get_Transform() == nullptr)
        return;

    _editorDesignPosition = _root->Get_Transform()->Get_Position();
    _hasEditorDesignPosition = true;
}

void UI_RuntimePanelBase::Reset_EditorDesignPositionCache()
{
    _editorDesignPosition = Vec2::Zero;
    _hasEditorDesignPosition = false;
}

void UI_RuntimePanelBase::Append_RuntimeUIMapEntries(
    vector<RuntimeUIEntry>& outEntries,
    const string& category,
    const umap<wstring, Shared<UI_Base>>& uiByName,
    const Shared<UI_Base>& rootToSkip,
    IRuntimeUI_EditorTarget* editorTarget)
{
    for (const auto& [uiName, ui] : uiByName)
    {
        if (ui == nullptr)
            continue;

        if (rootToSkip != nullptr && ui == rootToSkip)
            continue; // 루트는 상위 entry로 이미 등록하므로 중복 추가하지 않는다.

        RuntimeUIEntry entry{};
        entry.category = category;
        entry.name = string(uiName.begin(), uiName.end());
        entry.ui = ui;
        entry.editorTarget = editorTarget;
        outEntries.push_back(entry);
    }
}

void UI_RuntimePanelBase::Apply_EditorOffsets(Vec2& inOutPosition) const
{
    inOutPosition += _editorDefaultTestOffset; 
}

void UI_RuntimePanelBase::Free()
{
    Base::Free();
}
