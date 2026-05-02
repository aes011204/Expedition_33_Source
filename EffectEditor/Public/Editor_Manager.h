#pragma once
#include "Base.h"

NS_BEGIN(EffectEditor)

class Editor_Window;

class Editor_Manager final : public Base
{
public:
    Editor_Manager() = default;
    ~Editor_Manager() override;

public:
    HRESULT Initialize();
    void Update(float timeDelta);
    void Render();

    void Add_Window(const Shared<Editor_Window>& window); // View 추가
    Shared<Editor_Window> Find_Window(const wstring& windowName) const;

private:
    void Render_DockSpace();

private:
    // 창 이름을 키로 보관하는 에디터 뷰 목록.
    map<wstring, Shared<Editor_Window>> _windows{};

public:
    static Unique<Editor_Manager> Create();

    void Free() override;
};

NS_END
