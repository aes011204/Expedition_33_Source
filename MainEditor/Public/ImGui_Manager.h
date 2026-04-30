#pragma once

#include "Base.h"

NS_BEGIN(MainEditor)

class ImGui_Manager : public Base
{
public:
    ImGui_Manager() = default;
    ~ImGui_Manager();

public:
    HRESULT Initialize(HWND hWnd, ComPtr<Device> device, ComPtr<Context> context);
    void    Update(float timeDelta);
    void    Render();
    void    Free();

private:
    void    ImGuiStyleSetting();

public:
    static Unique<ImGui_Manager> Create(HWND hWnd, ComPtr<Device> device, ComPtr<Context> context);

};

NS_END

