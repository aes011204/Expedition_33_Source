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

    Close();
    return S_OK;
}

void UI_HealFont::Update(float timeDelta)
{
    if (!Is_Open() || !_isPlaying || !_hasStartScreenPos)
        return;

    _elapsed += timeDelta;

    const float safeLifeTime = max(_lifeTime, 0.0001f);
    const float t = clamp(_elapsed / safeLifeTime, 0.f, 1.f);

    Vec2 screenPos = _startScreenPos;
    Apply_StackOffset(screenPos);
    Apply_RiseOffset(screenPos, t);

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(screenPos);

    if (_elapsed >= _lifeTime)
        Close();
}

void UI_HealFont::Play_HealFont(const Vec2& screenPos, float healAmount, uint32 stackIndex)
{
    const int32 healValue = max(0, static_cast<int32>(healAmount));

    _startScreenPos = screenPos;
    Apply_EditorOffsets(_startScreenPos);
    _hasStartScreenPos = true;
    _stackIndex = stackIndex;
    _elapsed = 0.f;
    _isPlaying = true;

    Set_HealText(healValue);

    Vec2 startPos = _startScreenPos;
    Apply_StackOffset(startPos);

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(startPos);

    UI_RuntimePanelBase::Open();

    if (_root)
        _root->Play_UIAnimation(L"HealFont", true);
}

void UI_HealFont::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_HealFont::Close()
{
    _hasStartScreenPos = false;
    _isPlaying = false;
    _elapsed = 0.f;
    _stackIndex = 0;

    UI_RuntimePanelBase::Close();
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

void UI_HealFont::Apply_StackOffset(Vec2& screenPos) const
{
    const float horizontal = (_stackIndex % 2 == 0) ? -18.f : 18.f;
    const float vertical = -22.f * static_cast<float>(_stackIndex);

    screenPos.x += horizontal;
    screenPos.y += vertical;
}

void UI_HealFont::Apply_RiseOffset(Vec2& screenPos, float t) const
{
    const float eased = 1.f - (1.f - t) * (1.f - t);
    screenPos.y -= _riseDistance * eased;
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
    UI_RuntimePanelBase::Free();
}
