#include "UI_DestroyShield.h"

#include "Client_Constants.h"
#include "UI_Animation.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kStatusIconIntroAnimationName[] = L"Status_Icon_Intro.uianim";
}

HRESULT UI_DestroyShield::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Set_FloatingMotion(_lifeTimeSec, _riseDistance);
    Set_WorldAnchorOffset(_worldAnchorOffset);
    Set_ScreenBaseOffset(_screenBaseOffset);
    Set_StackStep(_stackHorizontalOffset, _stackVerticalOffset);
    Set_ScreenClamp(true, _screenClampPadding);
    Set_WorldAnchorRandomYRange(_worldAnchorRandomMinY, _worldAnchorRandomMaxY);
    Set_AnchorMode(AnchorMode::FollowWorld);

    Close();
    return S_OK;
}

void UI_DestroyShield::Update(float timeDelta)
{
    UI_BattleFloatingTextBase::Update(timeDelta);
}

void UI_DestroyShield::Close()
{
    UI_BattleFloatingTextBase::Close();
    _activeAttackType = AttackType::Invalid;
    _hasOverrideWorldAnchor = false;
    _overrideWorldAnchor = Vec3::Zero;
}

void UI_DestroyShield::Play_DestroyShield(
    const BattleChar& target,
    uint32 stackIndex,
    AttackType attackType,
    const Vec3* overrideWorldAnchor)
{
    _activeAttackType = attackType;
    _hasOverrideWorldAnchor = overrideWorldAnchor != nullptr;
    _overrideWorldAnchor = overrideWorldAnchor != nullptr ? *overrideWorldAnchor : Vec3::Zero;

    const bool isFreeAimDamage =
        _activeAttackType == AttackType::FreeAimLight ||
        _activeAttackType == AttackType::FreeAimWeak;
    const float stackHorizontalOffset = isFreeAimDamage ? _freeAimStackHorizontalOffset : _stackHorizontalOffset;
    Set_StackStep(stackHorizontalOffset, _stackVerticalOffset);

    if (!Begin_FloatingText(target, stackIndex))
        return;

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Scale(Vec2(_visualScale, _visualScale));

    if (_destroyShieldIcon)
        _destroyShieldIcon->Play_UIAnimation(kStatusIconIntroAnimationName, true);
}

bool UI_DestroyShield::Try_GetFeedbackWorldPosition(const BattleChar& target, Vec3& outWorldPos) const
{
    if (!_hasOverrideWorldAnchor)
        return UI_BattleFloatingTextBase::Try_GetFeedbackWorldPosition(target, outWorldPos);

    outWorldPos = _overrideWorldAnchor;
    outWorldPos += _worldAnchorOffset;
    outWorldPos.y += _worldAnchorRandomY;
    return true;
}

Vec2 UI_DestroyShield::Resolve_ScreenBaseOffset(const Vec3& worldPos) const
{
    const Vec4* cameraPosition = GAME->Get_CamPosition();
    if (cameraPosition == nullptr)
        return _screenBaseOffset;

    const Vec3 cameraWorldPos(cameraPosition->x, cameraPosition->y, cameraPosition->z);
    const float distance = (worldPos - cameraWorldPos).Length();
    const float range = max(_screenOffsetFarDistance - _screenOffsetCloseDistance, 0.0001f);
    const float farT = clamp((distance - _screenOffsetCloseDistance) / range, 0.f, 1.f);

    const bool isFreeAimDamage =
        _activeAttackType == AttackType::FreeAimLight ||
        _activeAttackType == AttackType::FreeAimWeak;
    const Vec2 farScreenBaseOffset = isFreeAimDamage ? _freeAimFarScreenBaseOffset : _farScreenBaseOffset;

    return _screenBaseOffset + (farScreenBaseOffset - _screenBaseOffset) * farT;
}

HRESULT UI_DestroyShield::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Destroy_Shield_Path;
    desc.prototypeTagPrefix = L"DestroyShield";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Image_Destroy_Shield");
    CHECK_NULL(_root, E_FAIL);

    UI_KeyframeAnimationAsset introAnimation{};
    if (!UI_RuntimeLoader::Load_PublicAnimationAsset(Constants::UI_Anim::UI_StatusIconIntroAnim_Path, introAnimation))
        return E_FAIL;

    _root->Add_UIAnimationAsset(introAnimation);

    return S_OK;
}

HRESULT UI_DestroyShield::Bind_UI()
{
    _destroyShieldIcon = Find_UI(_uiByName, L"Image_Destroy_Shield");
    CHECK_NULL(_destroyShieldIcon, E_FAIL);

    return S_OK;
}

Unique<UI_DestroyShield> UI_DestroyShield::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_DestroyShield>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_DestroyShield");
        return nullptr;
    }

    return instance;
}

void UI_DestroyShield::Free()
{
    _destroyShieldIcon.reset();
    UI_BattleFloatingTextBase::Free();
}
