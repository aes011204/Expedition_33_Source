#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class RenderTarget;
class UI_Base;
class UI_Image;
NS_END

NS_BEGIN(Client)

class UI_GradientSkillPreviewTest final : public UI_RuntimePanelBase
{
public:
    UI_GradientSkillPreviewTest() = default;
    ~UI_GradientSkillPreviewTest() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;
    void    Open() override;
    void    Close() override;

    void    Close_Immediately();
    Shared<UI_Base> Get_RootUI() const { return _root; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    HRESULT Ensure_RenderTarget();
    void    Bind_RenderTargetToImage();
    void    Update_RenderTarget(float timeDelta);
    bool    Try_RenderPreviewHeroToTarget(float timeDelta);
    void    Update_TestPatternRenderTarget(float timeDelta);
    void    Clear_RenderTargetImage();

private:
    uint32 _levelIndex = 0;
    Shared<UI_Base> _root{};
    Shared<UI_Image> _previewImage{};
    umap<wstring, Shared<UI_Base>> _uiByName{};

    Shared<Engine::RenderTarget> _previewTarget{};
    float _elapsedSec = 0.f;
    bool _previewResourcesPrepared = false;
    bool _previewAnimationStarted = false;

public:
    static Unique<UI_GradientSkillPreviewTest> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
