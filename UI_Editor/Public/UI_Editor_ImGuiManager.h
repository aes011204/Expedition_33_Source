#pragma once

#include "Base.h"

NS_BEGIN(UIEditor)

class UI_Editor_ImGuiManager final : public Base
{
public:
    UI_Editor_ImGuiManager() = default;
    ~UI_Editor_ImGuiManager() override;

public:
    HRESULT Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Update(float timeDelta);
    void Render();

private:
    void ShutDown();
    void Apply_Style();
    void Suppress_RepeatedPasteShortcut();

private:
    bool _initialized = false;

public:
    static Unique<UI_Editor_ImGuiManager> Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;
};

NS_END
