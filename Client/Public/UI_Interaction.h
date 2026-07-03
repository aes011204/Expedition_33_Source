#pragma once

#include <array>

#include "Client_Defines.h"
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Client)

class UI_Interaction final : public UI_RuntimePanelBase
{
public:
    UI_Interaction() = default;
    ~UI_Interaction() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;
    void    Close() override;

public:
    void    Show(InteractionPromptType type);
    void    Hide();
    void    Close_Immediately();

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Close_AllPromptPanels();
    void    Stop_RootAnimation();
    bool    Play_RootAnimation(const wchar_t* animationName);

    Shared<UI_Base> Find_PromptPanel(InteractionPromptType type) const;

private:
    static constexpr size_t kPromptPanelCount = static_cast<size_t>(InteractionPromptType::Down) + 1;

    std::array<Shared<UI_Base>, kPromptPanelCount> _promptPanels{};
    InteractionPromptType _currentType = InteractionPromptType::None;
    bool  _isClosingWithOutro = false;
    float _closeElapsed = 0.f;

public:
    static Unique<UI_Interaction> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
