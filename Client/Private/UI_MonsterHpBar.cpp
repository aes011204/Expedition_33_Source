#include "UI_MonsterHpBar.h"
#include "UI_Base.h"
#include "UI_Utils.h"
#include "GameInstance.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"

using namespace UI_Utils;

HRESULT UI_MonsterHpBar::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void UI_MonsterHpBar::Update(float timeDelta)
{
    if (!Is_Open() || !_hasTargetWorldPos)
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

void UI_MonsterHpBar::Set_TargetWorldPosition(const Vec3& worldPos)
{
    // GameplayUI가 넘긴 위치 저장
    _targetWorldPos = worldPos;
    _hasTargetWorldPos = true;
}

void UI_MonsterHpBar::Set_Hp(float curHp, float maxHp)
{
    maxHp = max(maxHp, 1.f);
    curHp = clamp(curHp, 0.f, maxHp);

    Apply_HpGauge(curHp / maxHp);
}

void UI_MonsterHpBar::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_MonsterHpBar::Close()
{
    _hasTargetWorldPos = false;

    UI_RuntimePanelBase::Close();
}

HRESULT UI_MonsterHpBar::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Monster_Hp_Path;
    desc.prototypeTagPrefix = L"MonsterHpBar";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_Monster_HP_Bar");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_MonsterHpBar::Bind_UI()
{
    Shared<UI_Base> hpGaugeBase = Find_UI(_uiByName, L"Image_Monster_HP_Gauge");
    CHECK_NULL(hpGaugeBase, E_FAIL);

    _hpGauge = dynamic_pointer_cast<UI_Image>(hpGaugeBase);
    CHECK_NULL(_hpGauge, E_FAIL);

    auto hpTr = _hpGauge->Get_Transform();
    CHECK_NULL(hpTr, E_FAIL);

    _hpGaugeBasePos = hpTr->Get_Position();
    _hpGaugeBaseSize = hpTr->Get_Size();

    return S_OK;
}

void UI_MonsterHpBar::Apply_HpGauge(float ratio)
{
    if (_hpGauge == nullptr)
        return;

    auto hpTr = _hpGauge->Get_Transform();
    if (hpTr == nullptr)
        return;

    ratio = ::clamp(ratio, 0.f, 1.f);

    const float newHpGauge = _hpGaugeBaseSize.x * ratio;
    hpTr->Set_Size(Vec2(newHpGauge, _hpGaugeBaseSize.y));
}

Unique<UI_MonsterHpBar> UI_MonsterHpBar::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_MonsterHpBar>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        MSG_BOX("Failed to Created : UI_MonsterHpBar");
        return nullptr;
    }

    return instance;
}

void UI_MonsterHpBar::Free()
{
    Base::Free();
}
