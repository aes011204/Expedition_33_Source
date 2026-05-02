#pragma once

#include "Base.h"

NS_BEGIN(MainEditor)

class ImGui_Manager final : public Base
{
public:
    ImGui_Manager() = default;
    ~ImGui_Manager() = default;

public:
    HRESULT Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void    Update(float timeDelta);
    void    Render();
    void    Free();

private:
    void    ImGuiStyleSetting();
    static string Get_ClientResourcePath(const wchar_t* relativePath);

public:
    static Unique<ImGui_Manager> Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);

};

NS_END

