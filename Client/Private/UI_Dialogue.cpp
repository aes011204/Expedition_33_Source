#include "UI_Dialogue.h"
#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Button.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

namespace
{
    constexpr wchar_t kDialogueIntroAnimName[] = L"Dialogue_Intro";
    constexpr wchar_t kChoiceIntroAnimName[] = L"Choice_Dialogue_Intro";
    constexpr wchar_t kFirstHoverAnimName[] = L"Choice_Dialogue_First_Button_Hover";
    constexpr wchar_t kFirstHoverCancelAnimName[] = L"Choice_Dialogue_First_Button_Hover_Cancel";
    constexpr wchar_t kSecondHoverAnimName[] = L"Choice_Dialogue_Second_Button_Hover";
    constexpr wchar_t kSecondHoverCancelAnimName[] = L"Choice_Dialogue_Second_Button_Hover_Cancel";
    constexpr wchar_t kChoicePanelUIName[] = L"Panel_Choice_Dialogue";
    constexpr wchar_t kDialoguePanelUIName[] = L"Panel_Dialogue";
    constexpr wchar_t kDialogueTextUIName[] = L"Text_Dialogue";
    constexpr wchar_t kFirstChoiceButtonUIName[] = L"Button_Choice_Dialogue_First";
    constexpr wchar_t kSecondChoiceButtonUIName[] = L"Button_Choice_Dialogue_Second";
    constexpr wchar_t kFirstChoiceTextUIName[] = L"Text_Choice_Dialogue_First";
    constexpr wchar_t kSecondChoiceTextUIName[] = L"Text_Choice_Dialogue_Second";
    constexpr wchar_t kNameTextUIName[] = L"Text_Dialogue_Interlocutor_Name";
    constexpr wchar_t kNameBGTextUIName[] = L"Text_Dialogue_Interlocutor_Name_BG";
}

void UI_Dialogue::Set_AllUIOpenState(bool isOpen)
{
    for (const auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        if (isOpen)
            ui->Open();
        else
            ui->Close();
    }
}

HRESULT UI_Dialogue::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void UI_Dialogue::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_Dialogue::Close()
{
    UI_RuntimePanelBase::Close();
    Set_AllUIOpenState(false);
}

void UI_Dialogue::Show_Dialogue()
{
    const bool wasClosed = !Is_Open();

    UI_RuntimePanelBase::Open();
    Set_AllUIOpenState(true);

    if (_dialoguePanel)
        _dialoguePanel->Open();

    if (_choicePanel)
        _choicePanel->Close();

    if (wasClosed)
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_Dialogue_CanvasEnter);
        Play_DialogueIntro();
    }
}

void UI_Dialogue::Show_Choices()
{
    const bool wasChoiceClosed = (_choicePanel == nullptr) ? false : !_choicePanel->Is_Open();

    //if (_dialoguePanel)
    //    _dialoguePanel->Open();

    if (_choicePanel)
        _choicePanel->Open();

    if (wasChoiceClosed)
        Play_ChoiceDialogueIntro();
}

void UI_Dialogue::Begin_FirstChoiceAnimation()
{
    if (_dialogueElements.firstChoiceButton == nullptr || _dialogueElements.secondChoiceButton == nullptr)
        return;

    _dialogueElements.firstChoiceButton->Play_UIAnimation(kFirstHoverAnimName, true);
    _dialogueElements.secondChoiceButton->Play_UIAnimation(kSecondHoverCancelAnimName, true);
}

void UI_Dialogue::Begin_SecondChoiceAnimation()
{
    if (_dialogueElements.firstChoiceButton == nullptr || _dialogueElements.secondChoiceButton == nullptr)
        return;

    _dialogueElements.firstChoiceButton->Play_UIAnimation(kFirstHoverCancelAnimName, true);
    _dialogueElements.secondChoiceButton->Play_UIAnimation(kSecondHoverAnimName, true);
}

void UI_Dialogue::Set_DialogueElements(
    const wstring& dialogueText,
    const wstring& firstChoice,
    const wstring& secondChoice,
    const wstring& nameText)
{
    if (_dialogueElements.dialogueText)
        _dialogueElements.dialogueText->Set_Text(dialogueText);

    if (_dialogueElements.firstChoiceText)
        _dialogueElements.firstChoiceText->Set_Text(firstChoice);

    if (_dialogueElements.secondChoiceText)
        _dialogueElements.secondChoiceText->Set_Text(secondChoice);

    if (_dialogueElements.nameText)
        _dialogueElements.nameText->Set_Text(nameText);

    if (_dialogueElements.nameBGText)
        _dialogueElements.nameBGText->Set_Text(nameText);
}

bool UI_Dialogue::Consume_ClickedChoice(int& outChoiceIndex) const
{
    outChoiceIndex = -1;

    if (_dialogueElements.firstChoiceButton &&
        _dialogueElements.firstChoiceButton->Was_Clicked())
    {
        outChoiceIndex = 0;
        return true;
    }

    if (_dialogueElements.secondChoiceButton &&
        _dialogueElements.secondChoiceButton->Was_Clicked())
    {
        outChoiceIndex = 1;
        return true;
    }

    return false;
}

void UI_Dialogue::Play_DialogueIntro()
{
    if (_dialoguePanel)
        _dialoguePanel->Play_UIAnimation(kDialogueIntroAnimName, false);
}

void UI_Dialogue::Play_ChoiceDialogueIntro()
{
    if (_dialoguePanel)
        _dialoguePanel->Play_UIAnimation(kChoiceIntroAnimName, false);

    if (_choicePanel)
        _choicePanel->Play_UIAnimation(kChoiceIntroAnimName, false);
}

HRESULT UI_Dialogue::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Dialogue;
    desc.prototypeTagPrefix = L"Dialogue";
    desc.outUIByName = &_uiByName;
    desc.animationPaths.push_back(Constants::UI_Anim::UI_Dialogue_Intro);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_Choice_Dialogue_First_Hover_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_Choice_Dialogue_First_Hover_Cancel_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_Choice_Dialogue_Intro);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_Choice_Dialogue_Second_Hover_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_Choice_Dialogue_Second_Hover_Cancel_Path);

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _choicePanel = UI_Utils::Find_UI(_uiByName, kChoicePanelUIName);
    CHECK_NULL(_choicePanel, E_FAIL);

    _dialoguePanel = UI_Utils::Find_UI(_uiByName, kDialoguePanelUIName);
    CHECK_NULL(_dialoguePanel, E_FAIL);
    _root = _dialoguePanel;

    return S_OK;
}

HRESULT UI_Dialogue::Bind_UI()
{
    _dialogueElements.dialogueText =
        UI_Utils::Find_TypedUI<UI_Text>(_uiByName, kDialogueTextUIName);
    _dialogueElements.firstChoiceButton =
        UI_Utils::Find_TypedUI<UI_Button>(_uiByName, kFirstChoiceButtonUIName);
    _dialogueElements.secondChoiceButton =
        UI_Utils::Find_TypedUI<UI_Button>(_uiByName, kSecondChoiceButtonUIName);
    _dialogueElements.firstChoiceText =
        UI_Utils::Find_TypedUI<UI_Text>(_uiByName, kFirstChoiceTextUIName);
    _dialogueElements.secondChoiceText =
        UI_Utils::Find_TypedUI<UI_Text>(_uiByName, kSecondChoiceTextUIName);
    _dialogueElements.nameText =
        UI_Utils::Find_TypedUI<UI_Text>(_uiByName, kNameTextUIName);
    _dialogueElements.nameBGText =
        UI_Utils::Find_TypedUI<UI_Text>(_uiByName, kNameBGTextUIName);

    CHECK_NULL(_dialogueElements.dialogueText, E_FAIL);
    CHECK_NULL(_dialogueElements.firstChoiceButton, E_FAIL);
    CHECK_NULL(_dialogueElements.secondChoiceButton, E_FAIL);
    CHECK_NULL(_dialogueElements.firstChoiceText, E_FAIL);
    CHECK_NULL(_dialogueElements.secondChoiceText, E_FAIL);
    CHECK_NULL(_dialogueElements.nameText, E_FAIL);
    CHECK_NULL(_dialogueElements.nameBGText, E_FAIL);

    return S_OK;
}

Unique<UI_Dialogue> UI_Dialogue::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_Dialogue>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_Dialogue");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_Dialogue::Free()
{
    _dialogueElements = {};
    _choicePanel.reset();
    _dialoguePanel.reset();
    _root.reset();

    UI_RuntimeLoader::Release_Document(_uiByName);

    UI_RuntimePanelBase::Free();
}
