#include "UI_HealFont.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

HRESULT UI_HealFont::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Set_FloatingMotion(0.85f, 28.f);
    Set_StackStep(18.f, 22.f);

    Set_WorldAnchorOffset(Vec3(0.f, 1.2f, 0.f));
    Set_ScreenBaseOffset(Vec2(0.f, -10.f));

    Set_WorldAnchorRandomYRange(-0.08f, 0.12f);

    Close();
    return S_OK;
}

void UI_HealFont::Play_HealFont(const BattleChar& target, float healAmount, uint32 stackIndex)
{
    const int32 healValue = max(0, static_cast<int32>(healAmount));

    Set_HealText(healValue);

    if (!Begin_FloatingText(target, stackIndex))
        return;

    if (_root)
        _root->Play_UIAnimation(L"HealFont", true);
}

void UI_HealFont::Open()
{
    UI_RuntimePanelBase::Open();
}

HRESULT UI_HealFont::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_HealFont_Path;
    desc.animationPath = Constants::UI_Anim::UI_HealFontAnim_Path;
    desc.prototypeTagPrefix = L"HealFont";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_HealFont");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_HealFont::Bind_UI()
{
    _healText = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_HealFont");
    _healTextBg = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_HealFont_BG");
    _plusText = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_HealFont_Plus");
    _plusTextBg = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_HealFont_Plus_BG");

    CHECK_NULL(_healText, E_FAIL);
    CHECK_NULL(_healTextBg, E_FAIL);
    CHECK_NULL(_plusText, E_FAIL);
    CHECK_NULL(_plusTextBg, E_FAIL);

    return S_OK;
}

void UI_HealFont::Set_HealText(int32 healValue)
{
    const wstring text = to_wstring(healValue);

    _healText->Set_Text(text);
    _healTextBg->Set_Text(text);
    _plusText->Set_Text(L"+");
    _plusTextBg->Set_Text(L"+");
}

Unique<UI_HealFont> UI_HealFont::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_HealFont>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_HealFont");
        return nullptr;
    }

    return instance;
}

void UI_HealFont::Free()
{
    UI_BattleFloatingTextBase::Free();
}
