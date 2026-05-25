#include "UI_ReactionResult.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_ReactionResult::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void UI_ReactionResult::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_ReactionResult::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_ReactionResult::Close()
{
    UI_RuntimePanelBase::Close();

    if (_dodgeRoot)
        _dodgeRoot->Close();

    if (_parryRoot)
        _parryRoot->Close();
}

void UI_ReactionResult::Show_Reaction(BattleController::ReactionType reactionType, const Vec2& screenPos)
{
    Open();

    if (_dodgeRoot)
        _dodgeRoot->Close();

    if (_parryRoot)
        _parryRoot->Close();

    Shared<UI_Base> selectedRoot{};

    if (reactionType == BattleController::ReactionType::Dodge)
        selectedRoot = _dodgeRoot;
    else if (reactionType == BattleController::ReactionType::Parry)
        selectedRoot = _parryRoot;

    if (selectedRoot == nullptr || selectedRoot->Get_Transform() == nullptr)
    {
        Close();
        return;
    }

    selectedRoot->Get_Transform()->Set_Position(screenPos);
    selectedRoot->Open();
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
