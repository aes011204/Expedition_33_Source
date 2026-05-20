#include "UI_RuntimePanelBase.h"
#include "UI_Base.h"

void UI_RuntimePanelBase::Open()
{
    _isOpen = true;

    if (_root)
        _root->Open();
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
