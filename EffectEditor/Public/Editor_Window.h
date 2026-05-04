#pragma once
#include <Base.h>

NS_BEGIN(EffectEditor)

class Editor_Window abstract : public Base
{
public:
    explicit Editor_Window(const wstring& windowName);
    ~Editor_Window() override = default;

public:
    virtual void Update(float timeDelta) = 0; // Editor_Manager가 매 프레임 창 상태를 갱신할 때 호출한다.
    virtual void Render();                    // Editor_Manager가 ImGui 프레임 안에서 이 창을 그릴 때 호출한다.

public:
    void Set_Open(bool isOpen);            // 메뉴나 툴바에서 창 표시 상태를 바꿀 때 호출한다.
    const wstring& Get_WindowName() const; // 도킹 메뉴에서 창 제목이나 검색 키가 필요할 때 호출한다.
    bool Is_Open() const;                  // 닫힌 창을 렌더링에서 건너뛰기 위해 호출한다.

protected:
    wstring _window{};    // 창 제목과 등록 키.
    bool _isOpen{ true }; // 현재 ImGui 창 열림 상태.

public:
    void Free() override;
};

NS_END
