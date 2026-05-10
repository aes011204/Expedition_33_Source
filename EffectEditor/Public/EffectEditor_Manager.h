#pragma once

#include "Base.h"

NS_BEGIN(EffectEditor)

class Editor_Window;

class EffectEditor_Manager final : public Base
{
public:
    EffectEditor_Manager() = default;
    ~EffectEditor_Manager() override;

public:
    HRESULT Initialize();
    void Update(float timeDelta);
    void Render();

    void Add_Window(const Shared<Editor_Window>& window);
    Shared<Editor_Window> Find_Window(const wstring& windowName) const;

private:
    map<wstring, Shared<Editor_Window>> _windows{};
    bool _defaultDockLayoutBuilt{ false };

private:
    // 등록된 editor window들의 초기화 hook을 한 번씩 실행한다.
    HRESULT Initialize_Windows();

    void Render_MainMenuBar();
    void Render_RuntimeToolbar();
    void Render_DockSpace();
    void Build_DefaultDockLayout(ImGuiID dockSpaceId, ImVec2 dockSpaceSize);
    void Handle_Shortcuts();
    void Render_ShortcutHelp();

    void Toggle_Window(const wstring& windowName);
    void Toggle_WindowPair(const wstring& firstWindowName, const wstring& secondWindowName);

    void Render_ExitButton(float toolbarHeight);
    void Request_Exit();

public:
    static Unique<EffectEditor_Manager> Create();
    void Free() override;
};

NS_END
