#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(MapEditor)

class MEWindow
{
public:
    explicit MEWindow(const wstring& name);
    virtual ~MEWindow() = default;

public:
    bool IsOpen() const { return _isOpen; }
    const wstring& GetName() const { return _name; }

    void SetOpen(bool open) { _isOpen = open; }

public:
    virtual void Update(float timeDelta);
    virtual void Render() = 0;

protected:
    wstring _name = {};
    bool _isOpen = true;
};

NS_END
