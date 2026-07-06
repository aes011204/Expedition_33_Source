#include "UI_BattleDefense.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Transform.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kUseDodgeAnimationName[] = L"Use_Dodge";
    constexpr wchar_t kUseParryAnimationName[] = L"Use_Parry";
    constexpr wchar_t kUseJumpAnimationName[] = L"Use_Jump";
}

HRESULT UI_BattleDefense::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    return S_OK;
}

void UI_BattleDefense::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_BattleDefense::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_BattleDefense::Close()
{
    Stop_UseAnimations();
    UI_RuntimePanelBase::Close();
}

void UI_BattleDefense::Play_UseAnimation(BattleReactionType reactionType)
{
    UseAnimationTarget* target = Resolve_UseAnimationTarget(reactionType);
    const wchar_t* animationName = Resolve_UseAnimationName(reactionType);
    if (target == nullptr || target->ui == nullptr || animationName == nullptr)
        return;

    Stop_UseAnimations();
    if (!target->ui->Play_UIAnimation(animationName, true))
        target->ui->Stop_UIAnimation();
}

HRESULT UI_BattleDefense::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Battle_Defense_Path;
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattleDefenseUseDodgeAnim_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattleDefenseUseParryAnim_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattleDefenseUseJumpAnim_Path);
    desc.prototypeTagPrefix = L"BattleDefense";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_Defense");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_BattleDefense::Bind_UI()
{
    CHECK_FAILED(Bind_UseAnimationTarget(L"Panel_BattleSystem_Dodge", _dodgeUseAnimTarget), E_FAIL);
    CHECK_FAILED(Bind_UseAnimationTarget(L"Panel_BattleSystem_Parry", _parryUseAnimTarget), E_FAIL);
    CHECK_FAILED(Bind_UseAnimationTarget(L"Panel_BattleSystem_Jump", _jumpUseAnimTarget), E_FAIL);

    return S_OK;
}

HRESULT UI_BattleDefense::Bind_UseAnimationTarget(const wchar_t* uiName, UseAnimationTarget& outTarget)
{
    outTarget.ui = Find_UI(_uiByName, uiName);
    CHECK_NULL(outTarget.ui, E_FAIL);

    if (outTarget.ui->Get_Transform() != nullptr)
        outTarget.baseScale = outTarget.ui->Get_Transform()->Get_Scale();

    return S_OK;
}

void UI_BattleDefense::Stop_UseAnimations()
{
    UseAnimationTarget* targets[] =
    {
        &_dodgeUseAnimTarget,
        &_parryUseAnimTarget,
        &_jumpUseAnimTarget,
    };

    for (UseAnimationTarget* target : targets)
    {
        if (target == nullptr || target->ui == nullptr)
            continue;

        target->ui->Stop_UIAnimation();

        if (target->ui->Get_Transform() != nullptr)
            target->ui->Get_Transform()->Set_Scale(target->baseScale);
    }
}

UI_BattleDefense::UseAnimationTarget* UI_BattleDefense::Resolve_UseAnimationTarget(BattleReactionType reactionType)
{
    switch (reactionType)
    {
    case BattleReactionType::Dodge:
        return &_dodgeUseAnimTarget;

    case BattleReactionType::Parry:
        return &_parryUseAnimTarget;

    case BattleReactionType::Jump:
        return &_jumpUseAnimTarget;

    default:
        return nullptr;
    }
}

const wchar_t* UI_BattleDefense::Resolve_UseAnimationName(BattleReactionType reactionType) const
{
    switch (reactionType)
    {
    case BattleReactionType::Dodge:
        return kUseDodgeAnimationName;

    case BattleReactionType::Parry:
        return kUseParryAnimationName;

    case BattleReactionType::Jump:
        return kUseJumpAnimationName;

    default:
        return nullptr;
    }
}

Unique<UI_BattleDefense> UI_BattleDefense::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleDefense>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleDefense");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleDefense::Free()
{
    Close();

    _dodgeUseAnimTarget = {};
    _parryUseAnimTarget = {};
    _jumpUseAnimTarget = {};
    _root.reset();
    _uiByName.clear();

    Base::Free();
}
