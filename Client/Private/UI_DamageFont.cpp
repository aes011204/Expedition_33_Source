#include "UI_DamageFont.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

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

void UI_DamageFont::Play_DamageFont(const BattleChar& target, float damage, uint32 stackIndex, bool isCritical, AttackType attackType)
{
    Apply_DamageFontTuning();
    _activeAttackType = attackType;

    auto bCriticalCheck = isCritical ? DamageFontStyle::Critical : DamageFontStyle::Normal;
    Set_ActiveStyle(bCriticalCheck);

    const int32 damageValue = max(0, static_cast<int32>(damage));
    Set_DamageText(damageValue);

    if (!Begin_FloatingText(target, stackIndex))
        return;

    if (_root)
    {
        const wstring animationName = isCritical ? L"CriticalDamageFont" : L"DamageFont";
        _root->Play_UIAnimation(animationName, true);
    }
}

void UI_DamageFont::Update(float timeDelta)
{
    Apply_DamageFontTuning();
    UI_BattleFloatingTextBase::Update(timeDelta);
}

void UI_DamageFont::Apply_EditorReflectionRuntimeTuning()
{
    if (Apply_DamageFontTuning() && !Is_Playing())
        Refresh_SnapshotScreenPosition();
}

void UI_DamageFont::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_DamageFont::Close()
{
    UI_BattleFloatingTextBase::Close();

    if (_normalRoot)
        _normalRoot->Close();

    if (_criticalRoot)
        _criticalRoot->Close();

    Set_ActiveStyle(DamageFontStyle::Normal); // 닫힌 뒤에 Normal로 세팅해주기
    _activeAttackType = AttackType::Invalid;
}

HRESULT UI_DamageFont::Load_UI(uint32 levelIndex)
{
    umap<wstring, Shared<UI_Base>> normalUIByName{};
    umap<wstring, Shared<UI_Base>> criticalUIByName{};

    UI_RuntimeLoader::UI_LoadDesc normalDesc{};
    normalDesc.levelIndex = levelIndex;
    normalDesc.jsonPath = Constants::UI::UI_NormalDamageFont_Path;
    normalDesc.animationPath = Constants::UI_Anim::UI_NormalDamageFontAnim_Path;
    normalDesc.prototypeTagPrefix = L"DamageFontNormal";
    normalDesc.outUIByName = &normalUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(normalDesc), E_FAIL);

    UI_RuntimeLoader::UI_LoadDesc criticalDesc{};
    criticalDesc.levelIndex = levelIndex;
    criticalDesc.jsonPath = Constants::UI::UI_CriticalDamageFont_Path;
    criticalDesc.animationPath = Constants::UI_Anim::UI_CriticalDamageFontAnim_Path;
    criticalDesc.prototypeTagPrefix = L"DamageFontCritical";
    criticalDesc.outUIByName = &criticalUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(criticalDesc), E_FAIL);

    _uiByName.clear();
    _uiByName.insert(normalUIByName.begin(), normalUIByName.end());
    _uiByName.insert(criticalUIByName.begin(), criticalUIByName.end());

    _normalRoot = Find_UI(normalUIByName, L"Panel_NormalDamageFont");
    _criticalRoot = Find_UI(criticalUIByName, L"Panel_CriticalDamageFont");

    CHECK_NULL(_normalRoot, E_FAIL);
    CHECK_NULL(_criticalRoot, E_FAIL);

    _root = _normalRoot;
    return S_OK;
}

HRESULT UI_DamageFont::Bind_UI()
{
    _normalDamageText = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_NormalDamageFont");
    _normalDamageTextBg = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_NormalDamageFont_BG");

    _criticalDamageText = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_CriticalDamageFont");
    _criticalDamageTextBg = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_CriticalDamageFont_BG");

    CHECK_NULL(_normalDamageText, E_FAIL);
    CHECK_NULL(_normalDamageTextBg, E_FAIL);
    CHECK_NULL(_criticalDamageText, E_FAIL);
    CHECK_NULL(_criticalDamageTextBg, E_FAIL);

    Set_ActiveStyle(DamageFontStyle::Normal);

    return S_OK;
}

void UI_DamageFont::Set_ActiveStyle(DamageFontStyle style)
{
    _activeStyle = style;

    if (_normalRoot)
        _normalRoot->Close();

    if (_criticalRoot)
        _criticalRoot->Close();

    if (_activeStyle == DamageFontStyle::Critical)
    {
        _root = _criticalRoot;
        _damageText = _criticalDamageText;
        _damageTextBg = _criticalDamageTextBg;
        return;
    }

    _root = _normalRoot;
    _damageText = _normalDamageText;
    _damageTextBg = _normalDamageTextBg;
}

void UI_DamageFont::Set_DamageText(int32 damageValue)
{
    const wstring text = to_wstring(damageValue);

    if (_damageText)
        _damageText->Set_Text(text);

    if (_damageTextBg)
        _damageTextBg->Set_Text(text);
}

Vec2 UI_DamageFont::Resolve_ScreenBaseOffset(const Vec3& worldPos) const
{
    const Vec4* cameraPosition = GAME->Get_CamPosition();
    if (cameraPosition == nullptr)
        return _damageScreenBaseOffset;

    const Vec3 cameraWorldPos(cameraPosition->x, cameraPosition->y, cameraPosition->z);
    const float distance = (worldPos - cameraWorldPos).Length();
    const float range = max(_damageScreenOffsetFarDistance - _damageScreenOffsetCloseDistance, 0.0001f);
    const float farT = clamp((distance - _damageScreenOffsetCloseDistance) / range, 0.f, 1.f);

    const bool isFreeAimDamage =
        _activeAttackType == AttackType::FreeAimLight ||
        _activeAttackType == AttackType::FreeAimWeak;
    const Vec2 farScreenBaseOffset = isFreeAimDamage ? _damageFreeAimFarScreenBaseOffset : _damageFarScreenBaseOffset;

    // 가까운 타격은 현재 높이를 유지하고, FreeAim 원거리 타격만 더 아래쪽으로 보정한다.
    return _damageScreenBaseOffset + (farScreenBaseOffset - _damageScreenBaseOffset) * farT;
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

    const bool isFreeAimDamage =
        _activeAttackType == AttackType::FreeAimLight ||
        _activeAttackType == AttackType::FreeAimWeak;
    const float stackHorizontalOffset = isFreeAimDamage ? _damageFreeAimStackHorizontalOffset : _damageStackHorizontalOffset;

    Set_FloatingMotion(_damageLifeTime, _damageRiseDistance);
    Set_StackStep(stackHorizontalOffset, _damageStackVerticalOffset);

    Set_WorldAnchorOffset(_damageWorldAnchorOffset);
    Set_ScreenBaseOffset(_damageScreenBaseOffset);
    Set_WorldAnchorRandomYRange(_damageWorldAnchorRandomMinY, _damageWorldAnchorRandomMaxY);
    Set_AnchorMode(AnchorMode::FollowWorld);

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
