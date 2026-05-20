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
    void    Set_HitState(bool isHit); // 히트 충돌중이면, 빨간색

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    void    Apply_Color();

private:
    Shared<UI_Image> _crossHairImage{};
    Shared<UI_Image> _crossHairOutlineImage{};

private:
    bool _isHit = false;

    Vec4 _normalColor = Vec4(1.f, 1.f, 1.f, 0.78f);
    Vec4 _hitColor = Vec4(1.f, 0.2f, 0.2f, 0.95f);
    Vec4 _outlineNormalColor = Vec4(1.f, 1.f, 1.f, 0.47f);
    Vec4 _outlineHitColor = Vec4(1.f, 0.35f, 0.35f, 0.70f);

public:
    static Unique<UI_CrossHair> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
