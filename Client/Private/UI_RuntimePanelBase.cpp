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

void UI_RuntimePanelBase::Apply_EditorOffsets(Vec2& inOutPosition) const
{
    inOutPosition += _editorDefaultTestOffset; 
}

void UI_RuntimePanelBase::Free()
{
    Base::Free();
}
