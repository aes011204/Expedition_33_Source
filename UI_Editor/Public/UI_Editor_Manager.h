#pragma once
#include "UI_Document.h"

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
    void Render_Hierarchy();
    void Render_Viewport();
    void Render_Inspector();

public:
    static Unique<UI_Editor_Manager> Create();

private:
    UI_Document _document = {};
    ObjectId _selectedObjectId = 0;
};

NS_END
