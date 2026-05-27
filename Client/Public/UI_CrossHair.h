#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Image;
NS_END

NS_BEGIN(Client)

class UI_CrossHair final : public UI_RuntimePanelBase
{
public:
    UI_CrossHair() = default;
    ~UI_CrossHair() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

public:
    void    Set_HitState(bool isHit);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Play_CrossAirAnimation(const wstring& animationName);

private:
    Shared<UI_Base> _outlineRoot{};
    Shared<UI_Image> _crossHairImage{};
    Shared<UI_Image> _crossHairOutlineImage{};

private:
    bool _isHit = false;
    bool _hasHitState = false;

public:
    static Unique<UI_CrossHair> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
