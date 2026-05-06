#include "UI_Manager.h"

#include <algorithm>

#include "UI_Base.h"
#include "Prototype_Manager.h"

UI_Manager::UI_Manager(Prototype_Manager* prototypeManager)
    : _prototypeManager{ prototypeManager }
{
}

HRESULT UI_Manager::Add_UI(uint32 prototypeLevelIndex, const wstring& prototypeTag, void* arg, Shared<UI_Base>* outUI)
{
    CHECK_NULL(_prototypeManager, E_FAIL);

    Shared<Base> clonedBase = _prototypeManager->Clone_Prototype(
        Prototype::UI,
        prototypeLevelIndex,
        prototypeTag,
        arg);

    Shared<UI_Base> ui = dynamic_pointer_cast<UI_Base>(clonedBase);
    CHECK_NULL(ui, E_FAIL);

    _uiList.push_back(ui);
    Sort_UIList();

    if (ui->Is_Open())
        _openedUIStack.push_back(ui);

    if (nullptr != outUI)
        *outUI = ui;

    return S_OK;
}


void UI_Manager::Sort_UIList()
{
    stable_sort(_uiList.begin(), _uiList.end(),
        [](const Shared<UI_Base>& leftUI, const Shared<UI_Base>& rightUI)
        {
            if (nullptr == leftUI)
                return false;

            if (nullptr == rightUI)
                return true;

            return leftUI->Get_SortOrder() < rightUI->Get_SortOrder();
        });
}


void UI_Manager::Pop_UI()
{
    while (!_openedUIStack.empty())
    {
        Shared<UI_Base> ui = _openedUIStack.back().lock();
        _openedUIStack.pop_back();

        if (nullptr == ui)
            continue;

        if (!ui->Is_Open())
            continue;

        ui->Close();
        return;
    }
}

HRESULT UI_Manager::Initialize()
{
    CHECK_NULL(_prototypeManager, E_FAIL);

    return S_OK;
}

void UI_Manager::Update(float timeDelta)
{
    for (auto& ui : _uiList)
    {
        if (nullptr == ui)
            continue;

        if (!ui->Is_Open())
            continue;

        ui->Update(timeDelta);
    }
}

HRESULT UI_Manager::Render()
{
    for (const auto& ui : _uiList)
    {
        if (nullptr == ui)
            continue;

        if (!ui->Is_Open())
            continue;

        if (FAILED(ui->Render()))
            return E_FAIL;
    }

    return S_OK;
}

void UI_Manager::Clear()
{
    for (auto& ui : _uiList)
    {
        if (nullptr != ui)
            ui->Free();
    }

    _uiList.clear();
    _openedUIStack.clear();
}

Unique<UI_Manager> UI_Manager::Create(Prototype_Manager* prototypeManager)
{
    Unique<UI_Manager> instance{ new UI_Manager(prototypeManager) };

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : UI_Manager");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_Manager::Free()
{
    Clear();

    __super::Free();
}
