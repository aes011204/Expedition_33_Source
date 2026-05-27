#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_HealFont final : public UI_RuntimePanelBase
{
public:
    UI_HealFont() = default;
    ~UI_HealFont() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

    void    Play_HealFont(const Vec2& screenPos, float healAmount, uint32 stackIndex);
    bool    Is_Playing() const { return _isPlaying; }
    float   Get_Elapsed() const { return _elapsed; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Set_HealText(int32 healValue);
    void    Apply_StackOffset(Vec2& screenPos) const;
    void    Apply_RiseOffset(Vec2& screenPos, float t) const;

private:
    Shared<UI_Text> _healText{};
    Shared<UI_Text> _healTextBg{};
    Shared<UI_Text> _plusText{};
    Shared<UI_Text> _plusTextBg{};

    Vec2  _startScreenPos = Vec2::Zero;
    bool  _hasStartScreenPos = false;

    bool  _isPlaying = false;
    float _elapsed = 0.f;
    float _lifeTime = 0.85f;
    float _riseDistance = 28.f;

    uint32 _stackIndex = 0;

public:
    static Unique<UI_HealFont> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
