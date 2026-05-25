#include "UI_DamageFont.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

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
    if (!Is_Open() || !_isPlaying || !_hasStartScreenPos)
        return;

    _elapsed += timeDelta;

    const float safeLifeTime = max(_lifeTime, 0.0001f);
    const float t = clamp(_elapsed / safeLifeTime, 0.f, 1.f);

    Vec2 screenPos = _startScreenPos;
    Apply_StackOffset(screenPos);      // 동시에 뜬 숫자끼리 겹치지 않게 살짝 벌린다.
    Apply_RiseOffset(screenPos, t);    // 시작 위치 기준으로만 위로 띄운다.

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(screenPos);

    if (_elapsed >= _lifeTime)
        Close();
}

void UI_DamageFont::Play_DamageFont(const Vec2& screenPos, float damage, uint32 stackIndex)
{
    const int32 damageValue = max(0, static_cast<int32>(damage));

    _startScreenPos = screenPos;
    _hasStartScreenPos = true;
    _stackIndex = stackIndex;
    _elapsed = 0.f;
    _isPlaying = true;

    Set_DamageText(damageValue);

    Vec2 startPos = _startScreenPos;
    Apply_StackOffset(startPos); // 첫 프레임부터 위치가 바로 맞도록 Open 전에 한 번 반영한다.

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(startPos);

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
    _hasStartScreenPos = false;
    _isPlaying = false;
    _elapsed = 0.f;
    _stackIndex = 0;

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

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_NormalDamageFont");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_DamageFont::Bind_UI()
{
    _damageText = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_NormalDamageFont");
    _damageTextBg = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_NormalDamageFont_BG");

    CHECK_NULL(_damageText, E_FAIL);
    CHECK_NULL(_damageTextBg, E_FAIL);

    return S_OK;
}

void UI_DamageFont::Set_DamageText(int32 damageValue)
{
    const wstring text = to_wstring(damageValue);

    _damageText->Set_Text(text);
    _damageTextBg->Set_Text(text);
}

void UI_DamageFont::Apply_StackOffset(Vec2& screenPos) const
{
    const float horizontal = (_stackIndex % 2 == 0) ? -18.f : 18.f;
    const float vertical = -22.f * static_cast<float>(_stackIndex);

    screenPos.x += horizontal;
    screenPos.y += vertical;
}

void UI_DamageFont::Apply_RiseOffset(Vec2& screenPos, float t) const
{
    const float eased = 1.f - (1.f - t) * (1.f - t); // 초반에 빨리 뜨고 후반에 살짝 완만해진다.
    screenPos.y -= _riseDistance * eased;
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
