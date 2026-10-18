#pragma once

#include "Base.h"

NS_BEGIN(MainEditor)

class ImGui_Manager final : public Base
{
public:
    ImGui_Manager() = default;
    ~ImGui_Manager() override;

public:
    HRESULT Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void    Update(float timeDelta);
    void    Render(bool renderPlatformWindows = true);
    void    EndFrameIfBegun();

private:
    void    ShutDown();
    void    ImGuiStyleSetting();
    static string Get_ClientResourcePath(const wchar_t* relativePath);

private:
    bool _initialized = false;
    bool _frameBegun = false;


public:
    static Unique<ImGui_Manager> Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;

};

NS_END

