#include "UI_ReactionResult.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr const wchar_t* kReactionResultIntroAnimationPath =
        L"UI/UI_Animations/BattleSystem/Status/StatusEffect_Intro";
    constexpr const wchar_t* kReactionResultIntroAnimationName = L"StatusEffect_Intro";

}

HRESULT UI_ReactionResult::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    UI_KeyframeAnimationAsset introAnimation{};
    if (UI_RuntimeLoader::Load_PublicAnimationAsset(kReactionResultIntroAnimationPath, introAnimation))
    {
        _dodgeRoot->Add_UIAnimationAsset(introAnimation);
        _parryRoot->Add_UIAnimationAsset(introAnimation);
    }

    Set_FloatingMotion(0.85f, 32.f);
    Set_StackStep(18.f, 20.f);
    Set_AnchorMode(AnchorMode::FollowWorld);
    Set_WorldAnchorOffset(Vec3(0.f, 0.55f, 0.f));
    Set_ScreenBaseOffset(Vec2(0.f, -20.f));
    Set_WorldAnchorRandomYRange(-0.04f, 0.08f);

    Close();

    return S_OK;
}

void UI_ReactionResult::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_ReactionResult::Close()
{
    UI_BattleFloatingTextBase::Close();

    if (_dodgeRoot)
        _dodgeRoot->Close();

    if (_parryRoot)
        _parryRoot->Close();

    _root.reset();
}

void UI_ReactionResult::Append_RuntimeUIEntries(
    vector<RuntimeUIEntry>& outEntries,
    const string& category,
    const string& rootName) const
{
    auto appendRoot = [&](const string& name, const Shared<UI_Base>& root)
    {
        if (root == nullptr)
            return;

        RuntimeUIEntry entry{};
        entry.category = category;
        entry.name = name;
        entry.ui = root;
        entry.editorTarget = const_cast<UI_ReactionResult*>(this);
        entry.positionDrivenByCode = Is_EditorPositionDrivenByCode();
        outEntries.push_back(entry);
    };

    appendRoot(rootName + "_Dodge", _dodgeRoot);
    appendRoot(rootName + "_Parry", _parryRoot);
}

void UI_ReactionResult::Show_Reaction(BattleReactionType reactionType, const BattleChar& target, uint32 stackIndex)
{
    Close();

    if (_dodgeRoot)
        _dodgeRoot->Close();

    if (_parryRoot)
        _parryRoot->Close();

    if (reactionType == BattleReactionType::Dodge)
        _root = _dodgeRoot;
    else if (reactionType == BattleReactionType::Parry)
        _root = _parryRoot;
    else
        return;

    if (_root == nullptr || _root->Get_Transform() == nullptr)
        return;

    _root->Open();

    if (!Begin_FloatingText(target, stackIndex))
        return;

    if (_root->Play_UIAnimation(kReactionResultIntroAnimationName, true))
        _root->Update(0.f);
}

HRESULT UI_ReactionResult::Load_UI(uint32 levelIndex)
{
    umap<wstring, Shared<UI_Base>> dodgeUIByName{};
    CHECK_FAILED(Load_UIFragment(
        Constants::UI::UI_Dodge_Path, L"DodgeResult", dodgeUIByName), E_FAIL);

    for (const auto& [name, ui] : dodgeUIByName)
        _uiByName[name] = ui;

    umap<wstring, Shared<UI_Base>> parryUIByName{};
    CHECK_FAILED(Load_UIFragment(
        Constants::UI::UI_Parry_Path, L"ParryResult",
        parryUIByName), E_FAIL);

    for (const auto& [name, ui] : parryUIByName)
        _uiByName[name] = ui;

    return S_OK;
}

HRESULT UI_ReactionResult::Load_UIFragment(
    const wchar_t* jsonPath,
    const wstring& prototypeTagPrefix,
    umap<wstring, Shared<UI_Base>>& outUIByName) const
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = _levelIndex;
    desc.jsonPath = jsonPath;
    desc.prototypeTagPrefix = prototypeTagPrefix;
    desc.outUIByName = &outUIByName;

    return UI_RuntimeLoader::Load_Document(desc);
}

HRESULT UI_ReactionResult::Bind_UI()
{
    _dodgeRoot = Find_UI(_uiByName, L"Panel_Dodge");
    _parryRoot = Find_UI(_uiByName, L"Panel_Parry");

    CHECK_NULL(_dodgeRoot, E_FAIL);
    CHECK_NULL(_parryRoot, E_FAIL);

    return S_OK;
}

Unique<UI_ReactionResult> UI_ReactionResult::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_ReactionResult>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_ReactionResult");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_ReactionResult::Free()
{
    if (_root)
        _root->Close();

    _dodgeRoot.reset();
    _parryRoot.reset();
    _root.reset();
    _uiByName.clear();

    Base::Free();
}
