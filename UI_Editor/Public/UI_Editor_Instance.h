#pragma once

#include "Base.h"
#include "UI_Editor_Defines.h"

NS_BEGIN(UIEditor)

class UI_Editor_Manager;
class UI_Editor_ImGuiManager;

class UI_Editor_Instance final : public Base
{
    DECLARE_SINGLETON(UI_Editor_Instance)

public:
    UI_Editor_Instance() = default;
    ~UI_Editor_Instance() override = default;

public:
    HRESULT Initialize_Editor(const UI_EDITOR_DESC& editorDesc, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Update_Editor(float timeDelta);
    void Render_Editor();
    void Render_FontPreviews();
    bool Request_Close();
    void Release_Editor();

public:
    UI_Editor_Manager* Get_Manager() const { return _editorManager.get(); }

private:
    ComPtr<Device> _device = nullptr;
    ComPtr<Context> _context = nullptr;
    UI_EDITOR_DESC _editorDesc = {};
    Unique<UI_Editor_Manager> _editorManager = nullptr;
    Unique<UI_Editor_ImGuiManager> _imguiManager = nullptr;

public:
    void Free() override;
};

NS_END
