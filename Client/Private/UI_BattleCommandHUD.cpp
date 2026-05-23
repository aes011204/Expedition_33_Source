#include "UI_BattleCommandHUD.h"
#include "Client_Constants.h"
#include "UI_RuntimeLoader.h"
#include "UI_Base.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BattleCommandHUD::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    return S_OK;
}

void UI_BattleCommandHUD::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_isOpen || !_hasTargetWorldPos)
        return;

    Vec2 screenPos{};
    if (!Try_ProjectWorldToScreen(_targetWorldPos, screenPos))
    {
        Close();
        return;
    }

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(screenPos);
}

void UI_BattleCommandHUD::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_BattleCommandHUD::Close()
{
    _hasTargetWorldPos = false;
    UI_RuntimePanelBase::Close();

    Apply_DefaultHighlight(); 
}

void UI_BattleCommandHUD::Refresh_ForState(BattleTurnState state)
{
    Apply_DefaultHighlight();

    switch (state)
    {
    case BattleTurnState::AttackTargeting:
        Set_ButtonHighlighted(_attackButton, true); // 기본 공격
        break;

    case BattleTurnState::ChooseSkill:
    case BattleTurnState::SkillTargeting:
        Set_ButtonHighlighted(_skillButton, true); // 스킬
        break;

    case BattleTurnState::ChooseItem:
    case BattleTurnState::ItemTargeting:
        Set_ButtonHighlighted(_itemButton, true); // 아이템
        break;

    case BattleTurnState::ChooseUltimate:
    case BattleTurnState::UltimateTargeting:
        Set_ButtonHighlighted(_shotButton, true); // 이건 일단 임시
        break;

    case BattleTurnState::ChooseAction:
    case BattleTurnState::None:
    default:
        break; // 커맨드 선택 대기 상태에서는 기본 상태 유지
    }
}

void UI_BattleCommandHUD::Set_TargetWorldPosition(const Vec3& worldPos)
{
    _targetWorldPos = worldPos;
    _hasTargetWorldPos = true;
}

HRESULT UI_BattleCommandHUD::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_BattleSystem_HUD_Path;
    desc.prototypeTagPrefix = L"BattleCommandHUD";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleCommand");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_BattleCommandHUD::Bind_UI()
{
    _attackButton = Find_UI(_uiByName, L"Button_BattleCommand_Attack");
    _skillButton  = Find_UI(_uiByName, L"Button_BattleCommand_Skill");
    _itemButton   = Find_UI(_uiByName, L"Button_BattleCommand_Item");
    _shotButton   = Find_UI(_uiByName, L"Button_BattleCommand_Shot");

    CHECK_NULL(_attackButton, E_FAIL);
    CHECK_NULL(_skillButton, E_FAIL);
    CHECK_NULL(_itemButton, E_FAIL);
    CHECK_NULL(_shotButton, E_FAIL);

    return S_OK;
}

void UI_BattleCommandHUD::Apply_DefaultHighlight()
{
    Set_ButtonHighlighted(_attackButton, false);
    Set_ButtonHighlighted(_skillButton, false);
    Set_ButtonHighlighted(_itemButton, false);
    Set_ButtonHighlighted(_shotButton, false);
}

void UI_BattleCommandHUD::Set_ButtonHighlighted(const Shared<UI_Base>& button, bool highlighted)
{
    if (button == nullptr || button->Get_Transform() == nullptr)
        return;

    // 선택 강조 표시
    button->Get_Transform()->Set_Scale(highlighted ? _highlightScale : _normalScale);
}

Unique<UI_BattleCommandHUD> UI_BattleCommandHUD::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleCommandHUD>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleCommandHUD");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleCommandHUD::Free()
{
    if (_root)
        _root->Close();

    _attackButton.reset();
    _skillButton.reset();
    _itemButton.reset();
    _shotButton.reset();
    _root.reset();
    _uiByName.clear();

    Base::Free();
}
