#include "UI_BattleCommandHUD.h"
#include "Client_Constants.h"
#include "UI_RuntimeLoader.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kBattleCommandIntroAnimationName[] = L"BattleCommand_Intro";
    constexpr wchar_t kBattleCommandLoopAnimationName[] = L"BattleCommand_Loop";

    float Resolve_BattleCommandIntroDurationSec()
    {
        UI_KeyframeAnimationAsset introAnimation{};
        introAnimation.fps = 60;
        introAnimation.startFrame = 0;
        introAnimation.endFrame = 60;
        return Get_KeyframeAnimationDurationSec(introAnimation);
    }
}

HRESULT UI_BattleCommandHUD::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    // BattleCommand HUD는 매 프레임 screen Pos 계산
    Set_EditorPositionDrivenByCode(true);
    _introDuration = Resolve_BattleCommandIntroDurationSec();

    Close();

    return S_OK;
}

void UI_BattleCommandHUD::Update(float timeDelta)
{
    if (!_isOpen || !_hasTargetWorldPos)
        return;

    if (_isPlayingIntro)
    {
        _introElapsed += max(0.f, timeDelta);

        if (_introElapsed >= _introDuration)
        {
            _isPlayingIntro = false;
            Play_HUDAnimation(kBattleCommandLoopAnimationName);
        }
    }

    Vec2 screenPos{};
    if (!Try_ProjectWorldToScreen(_targetWorldPos, screenPos))
    {
        Close();
        return;
    }

    // TEMP : Hero 별 오프셋 세팅 일단
    Apply_EditorOffsets(screenPos);
    screenPos += Resolve_HeroTestOffset(_currentHeroId);

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(screenPos);
}

void UI_BattleCommandHUD::Open()
{
    if (_root == nullptr)
        return;

    const bool wasClosed = !_root->Is_Open();

    UI_RuntimePanelBase::Open();

    if (!wasClosed)
        return;

    Open_AllHUDUIs(); // ChooseAction에 새로 진입한 순간에만 intro 대상들을 다시 연다.

    _isPlayingIntro = true;
    _introElapsed = 0.f;

    if (!Play_HUDAnimation(kBattleCommandIntroAnimationName))
    {
        _isPlayingIntro = false;
        Play_HUDAnimation(kBattleCommandLoopAnimationName);
    }
}

void UI_BattleCommandHUD::Close()
{
    _hasTargetWorldPos = false;
    _isPlayingIntro = false;
    _introElapsed = 0.f;
    _currentHeroId = HeroId::END;

    Stop_AllHUDAnimations();
    Close_AllHUDUIs();
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

void UI_BattleCommandHUD::Set_CurrentHeroId(HeroId heroId)
{
    _currentHeroId = heroId;
}

Vec2 UI_BattleCommandHUD::Get_EditorHeroTestOffset(HeroId heroId) const
{
    return Resolve_HeroTestOffset(heroId);
}

HRESULT UI_BattleCommandHUD::Render_DebugCapture()
{
    vector<Shared<UI_Base>> renderTargets{};
    renderTargets.reserve(_uiByName.size());

    for (const auto& [name, ui] : _uiByName)
    {
        if (ui == nullptr || !ui->Is_EffectivelyOpen())
            continue;

        const bool isRenderableCaptureTarget =
            dynamic_pointer_cast<UI_Image>(ui) != nullptr ||
            dynamic_pointer_cast<UI_Text>(ui) != nullptr;

        if (!isRenderableCaptureTarget)
            continue;

        renderTargets.push_back(ui);
    }

    stable_sort(renderTargets.begin(), renderTargets.end(),
        [](const Shared<UI_Base>& lhs, const Shared<UI_Base>& rhs)
        {
            if (lhs->Get_RenderGroup() != rhs->Get_RenderGroup())
                return static_cast<int32>(lhs->Get_RenderGroup()) < static_cast<int32>(rhs->Get_RenderGroup());

            if (lhs->Get_SortOrder() != rhs->Get_SortOrder())
                return lhs->Get_SortOrder() < rhs->Get_SortOrder();

            return lhs.get() < rhs.get();
        });

    for (const Shared<UI_Base>& ui : renderTargets)
        CHECK_FAILED(ui->Render(), E_FAIL);

    return S_OK;
}

void UI_BattleCommandHUD::Set_EditorHeroTestOffset(HeroId heroId, const Vec2& offset)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        _gustaveTestOffset = offset; // Gustave에만 적용할 추가 오프셋
        break;

    case HeroId::Lune:
        _luneTestOffset = offset; // Lune에만 적용할 추가 오프셋
        break;

    case HeroId::Maelle:
        _maelleTestOffset = offset; // Maelle에만 적용할 추가 오프셋
        break;

    case HeroId::END:
    default:
        break;
    }

    Mark_EditorRuntimeTuningDirty();
}

HRESULT UI_BattleCommandHUD::Load_UI(uint32 levelIndex)
{
    _allHUDUIs.clear();

    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_BattleSystem_HUD_Path;
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattleCommandIntroAnim_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattleCommandLoopAnim_Path);
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

    _allHUDUIs.clear();
    _allHUDUIs.reserve(8);

    auto Add_HUDAnimTarget = [this](const wchar_t* name) -> HRESULT
        {
            Shared<UI_Base> ui = Find_UI(_uiByName, name);
            CHECK_NULL(ui, E_FAIL);
            _allHUDUIs.push_back(ui);
            return S_OK;
    };

    CHECK_FAILED(Add_HUDAnimTarget(L"Panel_BattleCommand"), E_FAIL);
    CHECK_FAILED(Add_HUDAnimTarget(L"Button_BattleCommand_Shot"), E_FAIL);
    CHECK_FAILED(Add_HUDAnimTarget(L"Button_BattleCommand_Item"), E_FAIL);
    CHECK_FAILED(Add_HUDAnimTarget(L"Button_BattleCommand_Skill"), E_FAIL);
    CHECK_FAILED(Add_HUDAnimTarget(L"Button_BattleCommand_Attack"), E_FAIL);
    CHECK_FAILED(Add_HUDAnimTarget(L"Panel_BattlePortrait_Current_Character_AP_Bar"), E_FAIL);
    CHECK_FAILED(Add_HUDAnimTarget(L"Image_BattleCommand_BG_Deco"), E_FAIL);

    return S_OK;
}

void UI_BattleCommandHUD::Open_AllHUDUIs()
{
    for (const Shared<UI_Base>& ui : _allHUDUIs)
    {
        if (ui)
            ui->Open();
    }
}

void UI_BattleCommandHUD::Close_AllHUDUIs()
{
    for (const Shared<UI_Base>& ui : _allHUDUIs)
    {
        if (ui)
            ui->Close();
    }
}

void UI_BattleCommandHUD::Stop_AllHUDAnimations()
{
    for (const Shared<UI_Base>& ui : _allHUDUIs)
    {
        if (ui)
            ui->Stop_UIAnimation();
    }
}

bool UI_BattleCommandHUD::Play_HUDAnimation(const wchar_t* animationName)
{
    bool played = false;

    for (const Shared<UI_Base>& ui : _allHUDUIs)
    {
        if (ui == nullptr)
            continue;

        played = ui->Play_UIAnimation(animationName, true) || played;
    }

    return played;
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

Vec2 UI_BattleCommandHUD::Resolve_HeroTestOffset(HeroId heroId) const
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return _gustaveTestOffset;

    case HeroId::Lune:
        return _luneTestOffset;

    case HeroId::Maelle:
        return _maelleTestOffset;

    case HeroId::END:
    default:
        return Vec2::Zero;
    }
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
    Stop_AllHUDAnimations();
    Close_AllHUDUIs();

    _attackSlot = {};
    _skillSlot = {};
    _itemSlot = {};
    _shotSlot = {};

    _currentPlayerApText.reset();
    _currentPlayerApTextBg.reset();

    for (uint32 i = 0; i < kMaxCommandApSegmentCount; ++i)
        _currentPlayerApStates[i].reset();

    _root.reset();
    _allHUDUIs.clear();
    _uiByName.clear();

    Base::Free();
}
