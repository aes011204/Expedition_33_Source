#pragma once

NS_BEGIN(UIEditor)

class UI_Editor_Manager final
{
public:
    UI_Editor_Manager() = default;
    ~UI_Editor_Manager() = default;

public:
    HRESULT Initialize();
    void    Render();

private:
    void Render_DockSpace();
    void Render_Test_Window();

public:
    static Unique<UI_Editor_Manager> Create();
};

NS_END
