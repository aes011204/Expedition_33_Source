#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_AddAP : public UI_RuntimePanelBase
{
public:
    UI_AddAP() = default;
    ~UI_AddAP() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Close() override;

    void    Play_AddAP(const Vec2& screenPos, uint32 apAmount, uint32 stackIndex);
    bool    Is_Playing() const { return _isPlaying; }
    float   Get_Elapsed() const { return _elapsed; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Set_APText(uint32 apAmount);              
    void    Apply_StackOffset(Vec2& screenPos) const; 
    void    Apply_RiseOffset(Vec2& screenPos, float t) const;

private:
    Shared<UI_Text> _apText{};
    Shared<UI_Text> _apTextBg{};

    Vec2  _startScreenPos = Vec2::Zero;
    bool  _hasStartScreenPos = false;

    bool  _isPlaying = false;
    float _elapsed = 0.f;
    float _lifeTime = 1.0f;
    float _riseDistance = 32.f;
    uint32 _stackIndex = 0;

public:
    static Unique<UI_AddAP> Create(uint32 levelIndex);
    void Free() override;

};

NS_END
