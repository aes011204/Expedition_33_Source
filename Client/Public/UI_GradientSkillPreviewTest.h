#pragma once

#include "LightingPipeline.h"
#include "PostProcessPipeline.h"
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class ContainerObject;
class RenderTarget;
class UI_Base;
class UI_Image;
NS_END

NS_BEGIN(Client)

class Character_BattleBase;
class Monster_BattleBase;

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
    void    Apply_PreviewRenderOverrides();
    void    Restore_PreviewRenderOverrides();
    void    Apply_PreviewRimState();
    void    Clear_PreviewRimState();
    HRESULT Ensure_PreviewDioramaActors();
    void    Clear_PreviewDioramaActors();
    void    Layout_PreviewDioramaActors();
    void    Update_PreviewDioramaActors(float timeDelta);
    void    Start_PreviewSkillPlayback();
    void    Update_PreviewSkillPlayback(float timeDelta);
    vector<string> Build_PreviewAnimationKeys(const string& skillKey) const;
    HRESULT Render_PreviewActorParts(Engine::ContainerObject* actor);

private:
    struct PreviewRenderOverrideState
    {
        bool                        isCaptured = false;
        bool                        hasPrimaryLight = false;
        bool                        hasShadowLight = false;
        bool                        hasPostProcess = false;
        bool                        hasLighting = false;
        Engine::CascadeLevel        cascadeLevel = Engine::CascadeLevel::Normal;
        Engine::LIGHT_DESC          primaryLight{};
        Engine::ShadowLightDesc     shadowLight{};
        Engine::PostProcessCB       postProcess{};
        Engine::PostProcessFeature  postProcessFeatures = Engine::PostProcessFeature::None;
        Engine::DeferredLightingCB  deferredLighting{};
        bool                        iblEnabled = false;
    };

private:
    uint32 _levelIndex = 0;
    Shared<UI_Base> _root{};
    Shared<UI_Image> _previewImage{};
    umap<wstring, Shared<UI_Base>> _uiByName{};

    Shared<Engine::RenderTarget> _previewTarget{};
    float _elapsedSec = 0.f;
    bool _previewResourcesPrepared = false;
    PreviewRenderOverrideState _previewRenderOverrideState{};
    Shared<Character_BattleBase> _previewBattleHero{};
    Shared<Monster_BattleBase> _previewPotatoBag{};
    HeroId _previewHeroId = HeroId::Maelle;
    string _previewSkillKey = "Pyrolyse";
    vector<string> _previewSkillKeys{};
    size_t _previewSkillPhaseIndex = 0;
    float _previewSkillTimer = 0.f;
    bool _previewSkillPlaying = false;

public:
    static Unique<UI_GradientSkillPreviewTest> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
