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

    Close();
    return S_OK;
}

void UI_DestroyShield::Update(float timeDelta)
{
    UI_BattleFloatingTextBase::Update(timeDelta);
}

void UI_DestroyShield::Play_DestroyShield(const BattleChar& target, uint32 stackIndex)
{
    if (!Begin_FloatingText(target, stackIndex))
        return;

    if (_destroyShieldIcon)
        _destroyShieldIcon->Play_UIAnimation(kStatusIconIntroAnimationName, true);
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
