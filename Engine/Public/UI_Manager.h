#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Prototype_Manager;
class UI_Base;

class ENGINE_DLL UI_Manager final : public Base
{
public:
    UI_Manager(Prototype_Manager* prototypeManager);
    ~UI_Manager() override = default;

public:
    HRESULT Add_UI(
        uint32 prototypeLevelIndex,
        const wstring& prototypeTag,
        void* arg = nullptr,
        Shared<UI_Base>* outUI = nullptr);

    void Pop_UI();

public:
    HRESULT Initialize();
    HRESULT Render(UI_RenderGroup renderGroup = UI_RenderGroup::Foreground);
    void    Update(float timeDelta);
    void    Clear();
    void    Clear_Level(uint32 levelIndex);

private:
    void Sort_UIList();

private:
    struct UI_Entry
    {
        uint32 levelIndex = 0;
        Shared<UI_Base> ui = nullptr;
    };

    Prototype_Manager* _prototypeManager{ nullptr };
    vector<UI_Entry> _uiList{};
    vector<Weak<UI_Base>> _openedUIStack{};

public:
    static Unique<UI_Manager> Create(Prototype_Manager* prototypeManager);
    void Free() override;
};

NS_END
