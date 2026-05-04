#pragma once

#include "Base.h"

NS_BEGIN(MainEditor)

class Editor_Window abstract : public Base
{
public:
    Editor_Window(const wstring& windowName);
    virtual ~Editor_Window() override = default;

public:
    virtual HRESULT Initialize();
    virtual void    Update(float timeDelta);
    virtual void    Pre_Render();
    virtual void    Render();

    // 저장
    virtual bool    CanSave() const;
    virtual void    Save();

public:
    const wstring& Get_WindowName() const { return _window; }

    bool Is_Open() const { return _isOpen; }
    void Set_Open(bool isOpen) { _isOpen = isOpen; }

    bool Is_Focused() const { return _isFocused; }
    bool Is_Dirty() const { return _isDirty; }

    void MarkDirty() { _isDirty = true; }
    void ClearDirty() { _isDirty = false; }

    bool Is_Hovered() const { return _isHovered; }

protected:
    wstring _window;
    bool    _isOpen = true;
    bool    _isFocused = false;
    bool    _isDirty = false;
    bool    _isHovered = false;
};

NS_END
