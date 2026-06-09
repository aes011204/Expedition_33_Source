#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class UI_BattleTutorial : public UI_RuntimePanelBase
{
public:
    struct TutorialView
    {
        BattleTutorialStep step = BattleTutorialStep::END;

        const wchar_t* jsonPath = nullptr;
        const wchar_t* animationPath = nullptr;
        const wchar_t* rootName = nullptr;
        const wchar_t* animationName = nullptr;

        wstring prototypeTagPrefix{};
        Shared<UI_Base> root{};
        vector<Shared<UI_Base>> uiList{}; // 문서 단위로 열고 닫기 위해 Load_Document 결과를 보관한다.
    };

public:
    UI_BattleTutorial() = default;
    ~UI_BattleTutorial() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

    void    Show_Tutorial(const BattleTutorialRequest& request);
    bool    Is_ShowingTutorial() const { return _activeStep != BattleTutorialStep::END; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Load_TutorialView(TutorialView& view);
    HRESULT Bind_UI();

    void    Close_AllTutorialViews();
    TutorialView* Find_View(BattleTutorialStep step);

private:
    array<TutorialView, ETOI(BattleTutorialStep::END)> _views{};
    BattleTutorialStep _activeStep = BattleTutorialStep::END;

public:
    static Unique<UI_BattleTutorial> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
