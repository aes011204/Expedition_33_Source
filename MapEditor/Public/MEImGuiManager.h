#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(MapEditor)

class MEImGuiManager
{
public:
    MEImGuiManager() = default;
    ~MEImGuiManager();

public:
    HRESULT Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Update(float timeDelta);
    void Render();

private:
    void ApplyStyle();

public:
    static Unique<MEImGuiManager> Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
};

NS_END
