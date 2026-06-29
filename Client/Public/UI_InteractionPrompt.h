#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Client)

class UI_InteractionPrompt final : public UI_RuntimePanelBase
{
public:
    UI_InteractionPrompt() = default;
    ~UI_InteractionPrompt() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;
    void    Open() override;
    void    Close() override;

public:
    void    Show_AtWorldPosition(const Vec3& worldPosition);
    void    Clear_Target();

private:
    HRESULT Create_ImageUI(uint32 levelIndex);
    void    Apply_ScreenPlacement(const Vec2& screenPos);

public:
    static Unique<UI_InteractionPrompt> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
