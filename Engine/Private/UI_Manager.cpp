#include "UI_Manager.h"

#include <algorithm>
#include "Debug_Profile.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "Prototype_Manager.h"

namespace
{
bool Should_ProcessLevel(uint32 levelIndex)
{
    constexpr uint32 staticLevelIndex = 0;
    return levelIndex == staticLevelIndex ||
        levelIndex == GAME->Current_LevelIndex();
}
}

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

    _uiList.push_back(UI_Entry{ prototypeLevelIndex, ui });
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
        [](const UI_Entry& leftEntry, const UI_Entry& rightEntry)
        {
            const Shared<UI_Base>& leftUI = leftEntry.ui;
            const Shared<UI_Base>& rightUI = rightEntry.ui;

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
    ENGINE_PROFILE_SCOPE_BASIC("UI::UpdateAll");
#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
    double openCheckElapsedMs = 0.0;
    double actualUpdateElapsedMs = 0.0;
    const chrono::steady_clock::time_point loopBeginTime = chrono::steady_clock::now();
#endif

    for (auto& entry : _uiList)
    {
        if (!Should_ProcessLevel(entry.levelIndex))
            continue;

        Shared<UI_Base>& ui = entry.ui;

        if (nullptr == ui)
            continue;

#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
        const chrono::steady_clock::time_point openCheckBeginTime = chrono::steady_clock::now();
#endif

        const bool isEffectivelyOpen = ui->Is_EffectivelyOpen();
        ui->Notify_EffectiveOpenState(isEffectivelyOpen);

#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
        const chrono::steady_clock::time_point openCheckEndTime = chrono::steady_clock::now();
        const double openCheckMs = chrono::duration<double, milli>(openCheckEndTime - openCheckBeginTime).count();
        openCheckElapsedMs += openCheckMs;
#endif

        if (!isEffectivelyOpen)
            continue;

#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
        const chrono::steady_clock::time_point updateBeginTime = chrono::steady_clock::now();
#endif

        ui->Update(timeDelta);

#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
        const chrono::steady_clock::time_point updateEndTime = chrono::steady_clock::now();
        const double updateMs = chrono::duration<double, milli>(updateEndTime - updateBeginTime).count();
        actualUpdateElapsedMs += updateMs;
#endif
    }

#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
    const chrono::steady_clock::time_point loopEndTime = chrono::steady_clock::now();
    const double loopElapsedMs = chrono::duration<double, milli>(loopEndTime - loopBeginTime).count();
    const double loopOverheadMs = max(0.0, loopElapsedMs - openCheckElapsedMs - actualUpdateElapsedMs);

    ENGINE_PROFILE_PUSH_DETAIL("UI::OpenCheckAll", openCheckElapsedMs);
    ENGINE_PROFILE_PUSH_DETAIL("UI::ActualUpdateAll", actualUpdateElapsedMs);
    ENGINE_PROFILE_PUSH_DETAIL("UI::LoopOverheadAll", loopOverheadMs);
#endif
}

HRESULT UI_Manager::Render(UI_RenderGroup renderGroup)
{
    for (const auto& entry : _uiList)
    {
        if (!Should_ProcessLevel(entry.levelIndex))
            continue;

        const Shared<UI_Base>& ui = entry.ui;

        if (nullptr == ui)
            continue;

        if (!ui->Is_EffectivelyOpen())
            continue;

        if (ui->Get_RenderGroup() != renderGroup)
            continue;

        if (FAILED(ui->Render()))
            return E_FAIL;
    }

    return S_OK;
}

void UI_Manager::Clear()
{
    for (auto& entry : _uiList)
    {
        Shared<UI_Base>& ui = entry.ui;

        if (nullptr != ui)
            ui->Free();
    }

    _uiList.clear();
    _openedUIStack.clear();
}

void UI_Manager::Clear_Level(uint32 levelIndex)
{
    vector<UI_Base*> clearedUIs{};

    for (auto& entry : _uiList)
    {
        if (entry.levelIndex != levelIndex)
            continue;

        if (nullptr != entry.ui)
        {
            clearedUIs.push_back(entry.ui.get());
            entry.ui->Free();
        }
    }

    _uiList.erase(
        remove_if(_uiList.begin(), _uiList.end(),
            [levelIndex](const UI_Entry& entry)
            {
                return entry.levelIndex == levelIndex;
            }),
        _uiList.end());

    vector<Weak<UI_Base>> validStack{};
    validStack.reserve(_openedUIStack.size());

    for (const Weak<UI_Base>& weakUI : _openedUIStack)
    {
        Shared<UI_Base> ui = weakUI.lock();
        if (nullptr == ui)
            continue;

        if (find(clearedUIs.begin(), clearedUIs.end(), ui.get()) != clearedUIs.end())
            continue;

        validStack.push_back(weakUI);
    }

    _openedUIStack.swap(validStack);
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
