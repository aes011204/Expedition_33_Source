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

    constexpr Vec2 kReactionResultDodgeTransformOffset = Vec2(0.f, 0.f);
    constexpr Vec2 kReactionResultParryTransformOffset = Vec2(0.f, 200.f);

    Vec2 Resolve_ReactionResultTransformOffset(BattleReactionType reactionType)
    {
        switch (reactionType)
        {
        case BattleReactionType::Dodge:
            return kReactionResultDodgeTransformOffset;

        case BattleReactionType::Parry:
            return kReactionResultParryTransformOffset;

        default:
            return Vec2::Zero;
        }
    }
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

    Close();

    return S_OK;
}

void UI_ReactionResult::Update(float timeDelta)
{
    if (!Is_Open() || !_isShowing)
        return;

    _elapsed += timeDelta;

    if (_elapsed >= _lifeTime)
        Close(); // Dodge/Parry 성공 텍스트는 일정 시간 뒤 자동으로 닫는다.
}

void UI_ReactionResult::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_ReactionResult::Close()
{
    _isShowing = false;
    _elapsed = 0.f;

    UI_RuntimePanelBase::Close();

    if (_dodgeRoot)
    {
        _dodgeRoot->Close();
        if (_hasDodgeBaseTransformPosition && _dodgeRoot->Get_Transform() != nullptr)
            _dodgeRoot->Get_Transform()->Set_Position(_dodgeBaseTransformPosition);
    }

    if (_parryRoot)
    {
        _parryRoot->Close();
        if (_hasParryBaseTransformPosition && _parryRoot->Get_Transform() != nullptr)
            _parryRoot->Get_Transform()->Set_Position(_parryBaseTransformPosition);
    }
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

void UI_ReactionResult::Show_Reaction(BattleReactionType reactionType)
{
    const bool wasShowing = _isShowing;

    Open();
    _isShowing = true; // 위치 기반 리액션 텍스트의 표시 시간을 Update에서 관리한다.
    _elapsed = 0.f;

    if (_dodgeRoot)
        _dodgeRoot->Close();

    if (_parryRoot)
        _parryRoot->Close();

    Shared<UI_Base> selectedRoot{};
    Vec2* baseTransformPosition = nullptr;
    bool* hasBaseTransformPosition = nullptr;

    if (reactionType == BattleReactionType::Dodge)
    {
        selectedRoot = _dodgeRoot;
        baseTransformPosition = &_dodgeBaseTransformPosition;
        hasBaseTransformPosition = &_hasDodgeBaseTransformPosition;
    }
    else if (reactionType == BattleReactionType::Parry)
    {
        selectedRoot = _parryRoot;
        baseTransformPosition = &_parryBaseTransformPosition;
        hasBaseTransformPosition = &_hasParryBaseTransformPosition;
    }

    if (selectedRoot == nullptr ||
        selectedRoot->Get_Transform() == nullptr ||
        baseTransformPosition == nullptr ||
        hasBaseTransformPosition == nullptr)
    {
        Close();
        return;
    }

    if (!*hasBaseTransformPosition || !wasShowing)
    {
        *baseTransformPosition = selectedRoot->Get_Transform()->Get_Position();
        *hasBaseTransformPosition = true;
    }

    const Vec2 finalScreenPos = *baseTransformPosition + Resolve_ReactionResultTransformOffset(reactionType);
    selectedRoot->Open();
    selectedRoot->Get_Transform()->Set_Position(finalScreenPos);
    if (selectedRoot->Play_UIAnimation(kReactionResultIntroAnimationName, true))
        selectedRoot->Update(0.f);
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

    if (_dodgeRoot->Get_Transform() != nullptr)
    {
        _dodgeBaseTransformPosition = _dodgeRoot->Get_Transform()->Get_Position();
        _hasDodgeBaseTransformPosition = true;
    }

    if (_parryRoot->Get_Transform() != nullptr)
    {
        _parryBaseTransformPosition = _parryRoot->Get_Transform()->Get_Position();
        _hasParryBaseTransformPosition = true;
    }

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
