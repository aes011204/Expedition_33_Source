#include "UI_DamageFont.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

IMPLEMENT_REFLECTION(UI_DamageFont)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "UI_DamageFont";
    info.category = "Runtime UI";

    PROPERTY_FLOAT("Damage Life Time", _damageLifeTime, 0.1f, 5.f, 0.01f);
    PROPERTY_FLOAT("Damage Rise Distance", _damageRiseDistance, 0.f, 300.f, 1.f);

    PROPERTY_FLOAT("Damage Stack X", _damageStackHorizontalOffset, 0.f, 200.f, 1.f);
    PROPERTY_FLOAT("Damage Stack Y", _damageStackVerticalOffset, 0.f, 100.f, 1.f);

    PROPERTY_VEC3("Damage World Anchor Offset", _damageWorldAnchorOffset, 0.01f);
    PROPERTY_VEC2("Damage Screen Base Offset", _damageScreenBaseOffset, 1.f);

    PROPERTY_FLOAT("Damage Random Y Min", _damageWorldAnchorRandomMinY, -1.f, 1.f, 0.01f);
    PROPERTY_FLOAT("Damage Random Y Max", _damageWorldAnchorRandomMaxY, -1.f, 1.f, 0.01f);

    return true;
}

HRESULT UI_DamageFont::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Apply_DamageFontTuning();

    Close();
    return S_OK;
}

void UI_DamageFont::Play_DamageFont(const BattleChar& target, float damage, uint32 stackIndex)
{
    Apply_DamageFontTuning();

    const int32 damageValue = max(0, static_cast<int32>(damage));

    Set_DamageText(damageValue);

    if (!Begin_FloatingText(target, stackIndex))
        return;

    if (_root)
        _root->Play_UIAnimation(L"DamageFont", true);
}

void UI_DamageFont::Update(float timeDelta)
{
    if (Apply_DamageFontTuning())
        Refresh_SnapshotScreenPosition();

    UI_BattleFloatingTextBase::Update(timeDelta);
}

void UI_DamageFont::Apply_EditorReflectionRuntimeTuning()
{
    if (Apply_DamageFontTuning())
        Refresh_SnapshotScreenPosition();
}

void UI_DamageFont::Open()
{
    UI_RuntimePanelBase::Open();
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

bool UI_DamageFont::Apply_DamageFontTuning()
{
    const bool changed =
        !_hasAppliedDamageFontTuning ||
        _appliedDamageLifeTime != _damageLifeTime ||
        _appliedDamageRiseDistance != _damageRiseDistance ||
        _appliedDamageStackHorizontalOffset != _damageStackHorizontalOffset ||
        _appliedDamageStackVerticalOffset != _damageStackVerticalOffset ||
        _appliedDamageWorldAnchorOffset.x != _damageWorldAnchorOffset.x ||
        _appliedDamageWorldAnchorOffset.y != _damageWorldAnchorOffset.y ||
        _appliedDamageWorldAnchorOffset.z != _damageWorldAnchorOffset.z ||
        _appliedDamageScreenBaseOffset.x != _damageScreenBaseOffset.x ||
        _appliedDamageScreenBaseOffset.y != _damageScreenBaseOffset.y ||
        _appliedDamageWorldAnchorRandomMinY != _damageWorldAnchorRandomMinY ||
        _appliedDamageWorldAnchorRandomMaxY != _damageWorldAnchorRandomMaxY;

    Set_FloatingMotion(_damageLifeTime, _damageRiseDistance);
    Set_StackStep(_damageStackHorizontalOffset, _damageStackVerticalOffset);

    Set_WorldAnchorOffset(_damageWorldAnchorOffset);
    Set_ScreenBaseOffset(_damageScreenBaseOffset);
    Set_WorldAnchorRandomYRange(_damageWorldAnchorRandomMinY, _damageWorldAnchorRandomMaxY);

    _hasAppliedDamageFontTuning = true;
    _appliedDamageLifeTime = _damageLifeTime;
    _appliedDamageRiseDistance = _damageRiseDistance;
    _appliedDamageStackHorizontalOffset = _damageStackHorizontalOffset;
    _appliedDamageStackVerticalOffset = _damageStackVerticalOffset;
    _appliedDamageWorldAnchorOffset = _damageWorldAnchorOffset;
    _appliedDamageScreenBaseOffset = _damageScreenBaseOffset;
    _appliedDamageWorldAnchorRandomMinY = _damageWorldAnchorRandomMinY;
    _appliedDamageWorldAnchorRandomMaxY = _damageWorldAnchorRandomMaxY;

    return changed;
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
    UI_BattleFloatingTextBase::Free();
}
