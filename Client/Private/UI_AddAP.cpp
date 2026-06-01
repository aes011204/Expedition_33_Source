#include "UI_AddAP.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_AddAP::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void UI_AddAP::Update(float timeDelta)
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

void UI_AddAP::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_AddAP::Close()
{
    _hasStartScreenPos = false;
    _isPlaying = false;
    _elapsed = 0.f;
    _stackIndex = 0;

    UI_RuntimePanelBase::Close();
}

void UI_AddAP::Play_AddAP(const Vec2& screenPos, uint32 apAmount, uint32 stackIndex)
{
    _startScreenPos = screenPos;

    Apply_EditorOffsets(_startScreenPos);
    _hasStartScreenPos = true;
    _stackIndex = stackIndex;
    _elapsed = 0.f;
    _isPlaying = true;

    Set_APText(apAmount);

    Vec2 startPos = _startScreenPos;
    Apply_StackOffset(startPos);

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(startPos);

    UI_RuntimePanelBase::Open();

    if (_root)
        _root->Play_UIAnimation(L"Add_AP", true);
}

HRESULT UI_AddAP::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_BattleSystem_Add_AP_Path;
    desc.animationPath = Constants::UI_Anim::UI_AddAPAnim_Path;
    desc.prototypeTagPrefix = L"AddAP";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_BattleSystem_Add_AP");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_AddAP::Bind_UI()
{
    _apText = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_Add_AP");
    _apTextBg = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_Add_AP_BG");

    CHECK_NULL(_apText, E_FAIL);
    CHECK_NULL(_apTextBg, E_FAIL);

    return S_OK;
}

void UI_AddAP::Set_APText(uint32 apAmount)
{
    const wstring text = to_wstring(apAmount);

    _apText->Set_Text(text);
    _apTextBg->Set_Text(text);
}

void UI_AddAP::Apply_StackOffset(Vec2& screenPos) const
{
    const float horizontal = (_stackIndex % 2 == 0) ? -18.f : 18.f;
    const float vertical = -20.f * static_cast<float>(_stackIndex);

    screenPos.x += horizontal;
    screenPos.y += vertical;
}

void UI_AddAP::Apply_RiseOffset(Vec2& screenPos, float t) const
{
    const float eased = 1.f - (1.f - t) * (1.f - t);
    screenPos.y -= _riseDistance * eased;
}

Unique<UI_AddAP> UI_AddAP::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_AddAP>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        MSG_BOX("Failed to Created : UI_AddAP");
        return nullptr;
    }

    return instance;
}

void UI_AddAP::Free()
{
    UI_RuntimePanelBase::Free();
}
