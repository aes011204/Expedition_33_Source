#include "SavePoint_Panel.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

HRESULT SavePoint_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately(false);

    return S_OK;
}

void SavePoint_Panel::Update(float timeDelta)
{
    if (!_isOpen)
        return;

    if (_isClosingWithOutro)
    {
        _closeElapsed += max(0.f, timeDelta);

        if (_closeElapsed >= _outroDuration)
            Close_Immediately(_restoreWorldControlOnClose);

        return;
    }

    Handle_ButtonInput();
}

void SavePoint_Panel::Open()
{
    if (_root == nullptr)
        return;

    if (_isOpen && !_isClosingWithOutro)
        return;

    _isClosingWithOutro = false;
    _restoreWorldControlOnClose = true;
    _closeElapsed = 0.f;

    Stop_TargetAnimations();
    UI_RuntimePanelBase::Open();
    Play_TargetAnimation(L"SavePoint_Intro");
    GAME->Play_SoundCue(Constants::SoundCue::UI_SavePoint_Open);

    _lastAction = SavePointAction::None;

    // 마우스 잠금
    GAME->Unlock_Mouse();
    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);
}

void SavePoint_Panel::Close()
{
    Start_CloseWithOutro(true);
}

void SavePoint_Panel::Close_Immediately(bool restoreWorldControl)
{
    _isClosingWithOutro = false;
    _restoreWorldControlOnClose = true;
    _closeElapsed = 0.f;

    Stop_TargetAnimations();
    UI_RuntimePanelBase::Close();

    if (restoreWorldControl)
    {
        GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
        GAME->Lock_Mouse();
    }
}

void SavePoint_Panel::Close_ForMenuTransition()
{
    Close_Immediately(false);
}

void SavePoint_Panel::Toggle()
{
    if (_isOpen && !_isClosingWithOutro)
    {
        Close();
        return;
    }

    Open();
}

SavePoint_Panel::SavePointAction SavePoint_Panel::Consume_LastAction()
{
    const SavePointAction action = _lastAction;
    _lastAction = SavePointAction::None;

    return action;
}

HRESULT SavePoint_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_SavePoint_Path;
    desc.prototypeTagPrefix = L"SavePoint";
    desc.outUIByName = &_uiByName;
    desc.animationPaths =
    {
        Constants::UI_Anim::UI_SavePointIntro_Anim_Path,
        Constants::UI_Anim::UI_SavePointOutro_Anim_Path
    };

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_SavePoint");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT SavePoint_Panel::Bind_UI()
{
    _restButton = UI_Utils::Find_TypedUI<UI_Button>(_uiByName, L"Button_SavePoint_Rest");
    _upgradeStatButton = UI_Utils::Find_TypedUI<UI_Button>(_uiByName, L"Button_SavePoint_Upgrade_Stat");
    _upgradeSkillButton = UI_Utils::Find_TypedUI<UI_Button>(_uiByName, L"Button_SavePoint_Upgrade_Skill");

    CHECK_NULL(_restButton, E_FAIL);
    CHECK_NULL(_upgradeStatButton, E_FAIL);
    CHECK_NULL(_upgradeSkillButton, E_FAIL);

    return S_OK;
}

void SavePoint_Panel::Start_CloseWithOutro(bool restoreWorldControl)
{
    if (_root == nullptr)
    {
        Close_Immediately(restoreWorldControl);
        return;
    }

    if (!_isOpen && !_isClosingWithOutro)
        return;

    if (_isClosingWithOutro)
        return;

    _isClosingWithOutro = true;
    _restoreWorldControlOnClose = restoreWorldControl;
    _closeElapsed = 0.f;

    Play_TargetAnimation(L"SavePoint_Outro");
    GAME->Play_SoundCue(Constants::SoundCue::UI_SavePoint_Close);

    if (_outroDuration <= 0.f)
        Close_Immediately(_restoreWorldControlOnClose);
}

void SavePoint_Panel::Play_TargetAnimation(const wchar_t* animationName)
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Play_UIAnimation(animationName, true);
    }
}

void SavePoint_Panel::Stop_TargetAnimations()
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Stop_UIAnimation();
    }
}

void SavePoint_Panel::Handle_ButtonInput()
{
    if (_restButton && _restButton->Was_Clicked())
    {
        _lastAction = SavePointAction::Rest;
        LOG_DEBUG("SavePoint_Panel: Rest clicked");
        return;
    }

    if (_upgradeStatButton && _upgradeStatButton->Was_Clicked())
    {
        _lastAction = SavePointAction::UpgradeStat;
        LOG_DEBUG("SavePoint_Panel: UpgradeStat clicked");
        return;
    }

    if (_upgradeSkillButton && _upgradeSkillButton->Was_Clicked())
    {
        _lastAction = SavePointAction::UpgradeSkill;
        LOG_DEBUG("SavePoint_Panel: UpgradeSkill clicked");
        return;
    }
}

Unique<SavePoint_Panel> SavePoint_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<SavePoint_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : SavePoint_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void SavePoint_Panel::Free()
{
    if (_root)
        Close_Immediately(false);

    _restButton.reset();
    _upgradeStatButton.reset();
    _upgradeSkillButton.reset();
    _root.reset();
    _uiByName.clear();

    Base::Free();
}
