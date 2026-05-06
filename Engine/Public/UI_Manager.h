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
    HRESULT Render();
    void    Update(float timeDelta);
    void    Clear();

private:
    void Sort_UIList();

private:
    Prototype_Manager* _prototypeManager{ nullptr };
    vector<Shared<UI_Base>> _uiList{};
    vector<Weak<UI_Base>> _openedUIStack{};

public:
    static Unique<UI_Manager> Create(Prototype_Manager* prototypeManager);
    void Free() override;
};

NS_END
