#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

// 런타임에 보여줄 UI Panel Base -> SavePoint, CharacterStatus_Panel 등의 부모객체
class UI_RuntimePanelBase abstract : public Base
{
public:
    UI_RuntimePanelBase() = default;
    ~UI_RuntimePanelBase() override = default;

public:
    virtual HRESULT Initialize(uint32 levelIndex) = 0; 
    virtual void    Update(float timeDelta) = 0;       

public:
    virtual void Open();   
    virtual void Close();
    virtual void Toggle();

    bool Is_Open() const { return _isOpen; }
    Shared<UI_Base> Get_RootUI() const { return _root; }

protected:
    uint32 _levelIndex = 0; 
    bool   _isOpen = false; 

    umap<wstring, Shared<UI_Base>> _uiByName{};
    Shared<UI_Base> _root{};                   
};

NS_END
