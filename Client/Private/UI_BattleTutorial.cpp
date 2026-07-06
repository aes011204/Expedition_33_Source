#include "UI_BattleTutorial.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BattleTutorial::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void UI_BattleTutorial::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_BattleTutorial::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_BattleTutorial::Close()
{
    Close_AllTutorialViews();
    _activeStep = BattleTutorialStep::END;

    UI_RuntimePanelBase::Close();
}

void UI_BattleTutorial::Show_Tutorial(const BattleTutorialRequest& request)
{
    TutorialView* view = Find_View(request.step);
    if (!view || !view->root)
        return;

    Close();

    _activeStep = request.step;
    Open();
    GAME->Play_SoundCue(Constants::SoundCue::UI_Tuto_Appear);

    for (const Shared<UI_Base>& ui : view->uiList)
    {
        if (ui)
            ui->Open(); // 선택된 튜토리얼 문서의 자식/보조 패널까지 함께 연다.
    }

    view->root->Play_UIAnimation(view->animationName, true);
}

HRESULT UI_BattleTutorial::Load_UI(uint32 levelIndex)
{
    _views =
    {
        TutorialView{ BattleTutorialStep::BattleSystem, Constants::UI::UI_Tutorial_BattleSystem_Path, Constants::UI_Anim::UI_Tutorial_BattleSystemAnim_Path, L"Panel_Tutorial_BattleSystem", L"Tutorial_BattleSystem", L"TutorialBattleSystem" },
        TutorialView{ BattleTutorialStep::TurnOrder,    Constants::UI::UI_Tutorial_TurnOrder_Path,    Constants::UI_Anim::UI_Tutorial_TurnOrderAnim_Path,    L"Panel_Tutorial_TurnOrder",    L"Tutorial_TurnOrder",    L"TutorialTurnOrder" },
        TutorialView{ BattleTutorialStep::Attack,       Constants::UI::UI_Tutorial_Attack_Path,       Constants::UI_Anim::UI_Tutorial_AttackAnim_Path,       L"Panel_Tutorial_Attack",       L"Tutorial_Attack",       L"TutorialAttack" },
        TutorialView{ BattleTutorialStep::EnemyTurn,    Constants::UI::UI_Tutorial_EnemyTurn_Path,    Constants::UI_Anim::UI_Tutorial_EnemyTurnAnim_Path,    L"Panel_Tutorial_MonsterTurn",  L"Tutorial_MonsterTurn",  L"TutorialEnemyTurn" },
        TutorialView{ BattleTutorialStep::Dodge,        Constants::UI::UI_Tutorial_Dodge_Path,        Constants::UI_Anim::UI_Tutorial_DodgeAnim_Path,        L"Panel_Tutorial_Dodge",        L"Tutorial_Dodge",        L"TutorialDodge" },
        TutorialView{ BattleTutorialStep::Skill,        Constants::UI::UI_Tutorial_Skill_Path,        Constants::UI_Anim::UI_Tutorial_SkillAnim_Path,        L"Panel_Tutorial_Skill",        L"Tutorial_Skill",        L"TutorialSkill" },
        TutorialView{ BattleTutorialStep::QTE,          Constants::UI::UI_Tutorial_QTE_Path,          Constants::UI_Anim::UI_Tutorial_QTEAnim_Path,          L"Panel_Tutorial_QTE",          L"Tutorial_QTE",          L"TutorialQTE" },
        TutorialView{ BattleTutorialStep::Counter,      Constants::UI::UI_Tutorial_Counter_Path,      Constants::UI_Anim::UI_Tutorial_CounterAnim_Path,      L"Panel_Tutorial_Counter",      L"Tutorial_Counter",      L"TutorialCounter" },
        TutorialView{ BattleTutorialStep::FreeAim,      Constants::UI::UI_Tutorial_FreeAim_Path,      Constants::UI_Anim::UI_Tutorial_FreeAimAnim_Path,      L"Panel_Tutorial_FreeAim",      L"Tutorial_FreeAim",      L"TutorialFreeAim" },
        TutorialView{ BattleTutorialStep::Gradient,     Constants::UI::UI_Tutorial_Gradient_Path,     Constants::UI_Anim::UI_Tutorial_GradientAnim_Path,     L"Panel_Tutorial_Gradient",     L"Tutorial_Gradient",     L"TutorialGradient" },
    };

    for (TutorialView& view : _views)
    {
        CHECK_FAILED(Load_TutorialView(view), E_FAIL);
    }

    return S_OK;
}

HRESULT UI_BattleTutorial::Load_TutorialView(TutorialView& view)
{
    umap<wstring, Shared<UI_Base>> loadedUIByName{};

    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = _levelIndex;
    desc.jsonPath = view.jsonPath;
    desc.animationPath = view.animationPath;
    desc.prototypeTagPrefix = view.prototypeTagPrefix;
    desc.outUIByName = &loadedUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    for (const auto& [name, ui] : loadedUIByName)
    {
        _uiByName[name] = ui;
        view.uiList.push_back(ui); // Close/Show 시 이 튜토리얼 문서에 속한 UI만 제어한다.
    }

    return S_OK;
}

HRESULT UI_BattleTutorial::Bind_UI()
{
    for (TutorialView& view : _views)
    {
        view.root = Find_UI(_uiByName, view.rootName);
        CHECK_NULL(view.root, E_FAIL);
    }

    return S_OK;
}

void UI_BattleTutorial::Close_AllTutorialViews()
{
    for (const auto& [name, ui] : _uiByName)
    {
        if (ui == nullptr)
            continue;

        ui->Stop_UIAnimation();
        ui->Close();
    }
}

UI_BattleTutorial::TutorialView* UI_BattleTutorial::Find_View(BattleTutorialStep step)
{
    for (TutorialView& view : _views)
    {
        if (view.step == step)
            return &view;
    }

    return nullptr;
}

Unique<UI_BattleTutorial> UI_BattleTutorial::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleTutorial>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleTutorial");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleTutorial::Free()
{
    Close_AllTutorialViews();

    UI_RuntimePanelBase::Free();
}
