#include "UI_Interaction.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kRootName[] = L"Panel_Interaction";
    constexpr wchar_t kIntroAnimationName[] = L"Interaction_Intro";
    constexpr wchar_t kOutroAnimationName[] = L"Interaction_Outro";
    constexpr float kOutroDurationSec = 60.f / 45.f;

    size_t To_PromptPanelIndex(InteractionPromptType type)
    {
        return static_cast<size_t>(type);
    }

    const wchar_t* Resolve_PromptPanelName(InteractionPromptType type)
    {
        switch (type)
        {
        case InteractionPromptType::Dialogue:
            return L"Panel_Interaction_Dialogue";

        case InteractionPromptType::Picking:
            return L"Panel_Interaction_Picking";

        case InteractionPromptType::Cross:
            return L"Panel_Interaction_Cross";

        case InteractionPromptType::Up:
            return L"Panel_Interaction_Up";

        case InteractionPromptType::Down:
            return L"Panel_Interaction_Down";

        case InteractionPromptType::SavePoint:
            return L"Panel_Interaction_SavePoint";

        default:
            return nullptr;
        }
    }
}

HRESULT UI_Interaction::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately();

    return S_OK;
}

void UI_Interaction::Update(float timeDelta)
{
    if (!_isClosingWithOutro)
        return;

    _closeElapsed += max(0.f, timeDelta);
    if (_closeElapsed >= kOutroDurationSec)
        Close_Immediately();
}

void UI_Interaction::Close()
{
    Hide();
}

void UI_Interaction::Show(InteractionPromptType type)
{
    if (type == InteractionPromptType::None)
    {
        Hide();
        return;
    }

    if (_root == nullptr)
        return;

    if (Is_Open() && !_isClosingWithOutro && _currentType == type)
        return;

    _currentType = type;
    _isClosingWithOutro = false;
    _closeElapsed = 0.f;

    Stop_RootAnimation();
    UI_RuntimePanelBase::Open();

    Close_AllPromptPanels();

    const Shared<UI_Base> promptPanel = Find_PromptPanel(type);
    if (promptPanel)
        promptPanel->Open();

    Play_RootAnimation(kIntroAnimationName);
}

void UI_Interaction::Hide()
{
    if (_root == nullptr)
        return;

    if (!Is_Open() && !_isClosingWithOutro)
        return;

    if (_isClosingWithOutro)
        return;

    _currentType = InteractionPromptType::None;
    _isClosingWithOutro = true;
    _closeElapsed = 0.f;

    if (!Play_RootAnimation(kOutroAnimationName))
        Close_Immediately();
}

void UI_Interaction::Close_Immediately()
{
    _currentType = InteractionPromptType::None;
    _isClosingWithOutro = false;
    _closeElapsed = 0.f;

    Stop_RootAnimation();
    Close_AllPromptPanels();
    UI_RuntimePanelBase::Close();
}

HRESULT UI_Interaction::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Interaction_Path;
    desc.animationPath = Constants::UI_Anim::UI_InteractionIntroAnim_Path;
    desc.animationPaths.push_back(Constants::UI_Anim::UI_InteractionOutroAnim_Path);
    desc.prototypeTagPrefix = L"Interaction";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, kRootName);
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_Interaction::Bind_UI()
{
    const InteractionPromptType promptTypes[] =
    {
        InteractionPromptType::Dialogue,
        InteractionPromptType::Picking,
        InteractionPromptType::Cross,
        InteractionPromptType::Up,
        InteractionPromptType::Down,
        InteractionPromptType::SavePoint,
    };

    for (const InteractionPromptType type : promptTypes)
    {
        const wchar_t* panelName = Resolve_PromptPanelName(type);
        CHECK_NULL(panelName, E_FAIL);

        Shared<UI_Base> panel = Find_UI(_uiByName, panelName);
        CHECK_NULL(panel, E_FAIL);

        _promptPanels[To_PromptPanelIndex(type)] = panel;
    }

    return S_OK;
}

void UI_Interaction::Close_AllPromptPanels()
{
    for (const Shared<UI_Base>& panel : _promptPanels)
    {
        if (panel)
            panel->Close();
    }
}

void UI_Interaction::Stop_RootAnimation()
{
    if (_root)
        _root->Stop_UIAnimation();
}

bool UI_Interaction::Play_RootAnimation(const wchar_t* animationName)
{
    if (_root == nullptr || animationName == nullptr)
        return false;

    if (!_root->Play_UIAnimation(animationName, true))
    {
        _root->Stop_UIAnimation();
        return false;
    }

    _root->Update(0.f);
    return true;
}

Shared<UI_Base> UI_Interaction::Find_PromptPanel(InteractionPromptType type) const
{
    const size_t index = To_PromptPanelIndex(type);
    if (index >= _promptPanels.size())
        return nullptr;

    return _promptPanels[index];
}

Unique<UI_Interaction> UI_Interaction::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_Interaction>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_Interaction");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_Interaction::Free()
{
    Close_Immediately();

    _promptPanels = {};
    _root.reset();
    _uiByName.clear();

    UI_RuntimePanelBase::Free();
}
