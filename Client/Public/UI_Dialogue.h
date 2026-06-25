#pragma once
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Button;
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_Dialogue : public UI_RuntimePanelBase
{
public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;
    void    Close() override;
    void    Show_Dialogue();
    void    Show_Choices();
    void    Begin_FirstChoiceAnimation();
    void    Begin_SecondChoiceAnimation();
    bool    Consume_ClickedChoice(int& outChoiceIndex) const;

    void    Set_DialogueElements(const wstring& dialogueText, const wstring& firstChoice, const wstring& secondChoice, const wstring& nameText);

public:
    void    Play_DialogueIntro();
    void    Play_ChoiceDialogueIntro();

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    void    Set_AllUIOpenState(bool isOpen);

private:
    struct DialogueUIElements
    {
        Shared<UI_Text> dialogueText = {};
        Shared<UI_Button> firstChoiceButton = {};
        Shared<UI_Button> secondChoiceButton = {};
        Shared<UI_Text> firstChoiceText = {};
        Shared<UI_Text> secondChoiceText = {};
        Shared<UI_Text> nameText = {};
        Shared<UI_Text> nameBGText = {};
    };

    Shared<UI_Base> _choicePanel = {};
    Shared<UI_Base> _dialoguePanel = {};
    DialogueUIElements _dialogueElements = {};

public:
    static Unique<UI_Dialogue> Create(uint32 levelIndex);
    void Free() override;
};



NS_END
