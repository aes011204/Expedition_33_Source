#include "UI_BattleCommandHUD.h"
#include "Client_Constants.h"
#include "UI_RuntimeLoader.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_Text.h"
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
        Set_CommandSlotHighlighted(_attackSlot, true); // 기본 공격
        break;

    case BattleTurnState::ChooseSkill:
    case BattleTurnState::SkillTargeting:
        Set_CommandSlotHighlighted(_skillSlot, true); // 스킬
        break;

    case BattleTurnState::ChooseItem:
    case BattleTurnState::ItemTargeting:
        Set_CommandSlotHighlighted(_itemSlot, true); // 아이템
        break;

    case BattleTurnState::ChooseUltimate:
    case BattleTurnState::UltimateTargeting:
        Set_CommandSlotHighlighted(_shotSlot, true); // 이건 일단 임시
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

void UI_BattleCommandHUD::Set_CurrentPlayerAP(uint32 curAp, uint32 maxAp)
{
    maxAp = min(maxAp, kMaxCommandApSegmentCount);
    curAp = min(curAp, maxAp);

    Apply_CurrentPlayerAPGauge(curAp, maxAp);
    Set_CurrentPlayerAPText(curAp);
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
    CHECK_FAILED(Bind_CommandSlot(L"Attack", L"Panel_Key_F", true, _attackSlot), E_FAIL);
    CHECK_FAILED(Bind_CommandSlot(L"Skill", L"Panel_Key_E", true, _skillSlot), E_FAIL);
    CHECK_FAILED(Bind_CommandSlot(L"Item", L"Panel_Key_W", true, _itemSlot), E_FAIL);
    CHECK_FAILED(Bind_CommandSlot(L"Shot", L"Image_Input_RightMouse", false, _shotSlot), E_FAIL);
    CHECK_FAILED(Bind_CurrentPlayerAP(), E_FAIL);

    return S_OK;
}

void UI_BattleCommandHUD::Apply_DefaultHighlight()
{
    Set_CommandSlotHighlighted(_attackSlot, false);
    Set_CommandSlotHighlighted(_skillSlot, false);
    Set_CommandSlotHighlighted(_itemSlot, false);
    Set_CommandSlotHighlighted(_shotSlot, false);
}

HRESULT UI_BattleCommandHUD::Bind_CurrentPlayerAP()
{
    _currentPlayerApText =
        Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_Current_Character_Icon_AP");
    _currentPlayerApTextBg =
        Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_Current_Character_Icon_AP_BG");

    CHECK_NULL(_currentPlayerApText, E_FAIL);
    CHECK_NULL(_currentPlayerApTextBg, E_FAIL);

    for (uint32 apIndex = 0; apIndex < kMaxCommandApSegmentCount; ++apIndex)
    {
        _currentPlayerApStates[apIndex] =
            Find_TypedUI<UI_Image>(
                _uiByName,
                L"Image_BattlePortrait_Current_Character_AP_" + to_wstring(apIndex + 1));

        CHECK_NULL(_currentPlayerApStates[apIndex], E_FAIL);
    }

    return S_OK;
}

void UI_BattleCommandHUD::Apply_CurrentPlayerAPGauge(uint32 curAp, uint32 maxAp)
{
    const uint32 clampedMaxAp = min(maxAp, kMaxCommandApSegmentCount);

    for (uint32 i = 0; i < kMaxCommandApSegmentCount; ++i)
    {
        if (_currentPlayerApStates[i] == nullptr)
            continue;

        if (i < curAp && i < clampedMaxAp)
            _currentPlayerApStates[i]->Open();
        else
            _currentPlayerApStates[i]->Close();
    }
}

void UI_BattleCommandHUD::Set_CurrentPlayerAPText(uint32 curAp)
{
    const wstring apText = to_wstring(curAp);

    if (_currentPlayerApText)
        _currentPlayerApText->Set_Text(apText);

    if (_currentPlayerApTextBg)
        _currentPlayerApTextBg->Set_Text(apText);
}

HRESULT UI_BattleCommandHUD::Bind_CommandSlot(
    const wstring& commandName,
    const wstring& keyName,
    bool requireKey,
    CommandSlot& outSlot)
{
    const wstring prefix = L"BattleCommand_" + commandName;

    outSlot.root = Find_UI(_uiByName, L"Button_" + prefix);
    outSlot.frameBG = Find_UI(_uiByName, L"Image_" + prefix + L"_Frame_BG");
    outSlot.frameLeft = Find_UI(_uiByName, L"Image_" + prefix + L"_Frame_Left");
    outSlot.frameRight = Find_UI(_uiByName, L"Image_" + prefix + L"_Frame_Right");
    outSlot.text = Find_UI(_uiByName, L"Text_" + prefix);
    outSlot.key = keyName.empty() ? nullptr : Find_UI(_uiByName, keyName);

    CHECK_NULL(outSlot.root, E_FAIL);
    CHECK_NULL(outSlot.frameBG, E_FAIL);
    CHECK_NULL(outSlot.frameLeft, E_FAIL);
    CHECK_NULL(outSlot.frameRight, E_FAIL);
    CHECK_NULL(outSlot.text, E_FAIL);

    if (requireKey)
        CHECK_NULL(outSlot.key, E_FAIL);

    return S_OK;
}

void UI_BattleCommandHUD::Set_CommandSlotHighlighted(const CommandSlot& slot, bool highlighted)
{
    if (slot.root == nullptr || slot.root->Get_Transform() == nullptr)
        return;

    // 선택 강조 표시
    slot.root->Get_Transform()->Set_Scale(highlighted ? _highlightScale : _normalScale);
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

    _attackSlot = {};
    _skillSlot = {};
    _itemSlot = {};
    _shotSlot = {};

    _currentPlayerApText.reset();
    _currentPlayerApTextBg.reset();

    for (uint32 i = 0; i < kMaxCommandApSegmentCount; ++i)
        _currentPlayerApStates[i].reset();

    _root.reset();
    _uiByName.clear();

    Base::Free();
}
