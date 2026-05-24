// Client/Private/UI_DamageFont.cpp
#include "UI_DamageFont.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_DamageFont::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    return S_OK;
}

void UI_DamageFont::Update(float timeDelta)
{
    if (!Is_Open() || !_isPlaying || !_hasTargetWorldPos)
        return;

    _elapsed += timeDelta;

    Vec2 screenPos{};
    if (!Try_ProjectWorldToScreen(_targetWorldPos, screenPos))
    {
        Close();
        return;
    }

    Apply_ScreenOffset(screenPos);

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(screenPos);

    if (_elapsed >= _lifeTime)
        Close();
}

void UI_DamageFont::Play_DamageFont(const Vec3& worldPos, float damage, uint32 stackIndex)
{
    const int32 damageValue = max(0, static_cast<int32>(damage));

    _targetWorldPos = worldPos;
    _hasTargetWorldPos = true;
    _stackIndex = stackIndex;
    _elapsed = 0.f;
    _isPlaying = true;

    Set_DamageText(damageValue);

    UI_RuntimePanelBase::Open();

    if (_root)
        _root->Play_UIAnimation(L"DamageFont", true);
}

void UI_DamageFont::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_DamageFont::Close()
{
    _hasTargetWorldPos = false;
    _isPlaying = false;
    _elapsed = 0.f;

    UI_RuntimePanelBase::Close();
}

HRESULT UI_DamageFont::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_NormalDamageFont_Path;
    desc.animationPath = Constants::UI_Anim::UI_NormalDamageFontAnim_Path;
    desc.prototypeTagPrefix = L"DamageFont";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_NormalDamageFont");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_DamageFont::Bind_UI()
{
    _damageText = Find_TypedUI<UI_Text>(_uiByName, L"Text_NormalDamageFont");
    _damageTextBg = Find_TypedUI<UI_Text>(_uiByName, L"Text_NormalDamageFont_BG");

    CHECK_NULL(_damageText, E_FAIL);
    CHECK_NULL(_damageTextBg, E_FAIL);

    return S_OK;
}

void UI_DamageFont::Set_DamageText(int32 damageValue)
{
    const wstring text = to_wstring(damageValue);

    _damageText->Set_Text(text);   // 앞 숫자
    _damageTextBg->Set_Text(text); // 외곽/배경 숫자
}

void UI_DamageFont::Apply_ScreenOffset(Vec2& screenPos) const
{
    const float horizontal = (_stackIndex % 2 == 0) ? -18.f : 18.f;  // 좌우를 번갈아 흔들어 겹침을 줄인다.
    const float vertical = -22.f * static_cast<float>(_stackIndex);  // 뒤에 들어온 숫자는 위쪽으로 조금씩 더 벌린다.

    screenPos.x += horizontal;
    screenPos.y += vertical;
}

Unique<UI_DamageFont> UI_DamageFont::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_DamageFont>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_DamageFont");
        return nullptr;
    }

    return instance;
}

void UI_DamageFont::Free()
{
    UI_RuntimePanelBase::Free();
}
