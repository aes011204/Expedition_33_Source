#pragma once

#include "Client_Defines.h"
#include "Client_Enum.h"
#include "UIPreviewManager.h"
#include "Shader_CBuffer_Define.h"
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
NS_END

NS_BEGIN(Client)

class Wardrobe_Panel final : public UI_RuntimePanelBase
{
public:
    Wardrobe_Panel() = default;
    ~Wardrobe_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;
    void    Close_Immediately(bool restoreWorldControl = true);
    void    Close_ForMenuTransition();

    void    Set_SelectedHeroId(HeroId heroId);
    HeroId  Get_SelectedHeroId() const { return _selectedHeroId; }

    Shared<UI_Base> Get_RootUI() const { return _root; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Handle_SelectionInput();      // Q/E + 상단 캐릭터 버튼으로 현재 영웅 전환
    void    Handle_WardrobeInput();       // 카드 클릭 -> 장착/로그 처리
    void    Refresh_VisibleWardrobeCards();

    void    Sync_CurrentCostumeToPreviewHeroes();         // open 직후 preview spawn 결과를 현재 costume 상태와 맞춘다.
    void    Sync_CurrentCostumeToPreviewHero(HeroId heroId);
    bool    Try_EquipPreset(const char* presetId);        // Default/Baguette 클릭 시 manager + world + preview를 같이 갱신한다.

    void    Apply_PreviewOverrides();
    void    Restore_PreviewOverrides();
    void    Apply_PreviewRimState();
    void    Clear_PreviewRimState();
    void    Update_PreviewHeroVisibility();

    static HeroId Resolve_DefaultHeroId();

private:
    struct HeroWardrobeUI
    {
        array<Shared<UI_Base>, 3> panels{};
        array<Shared<UI_Button>, 3> buttons{};
    };

    struct PreviewOverrideState
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
    HeroId _selectedHeroId = HeroId::END;

    Shared<UI_Base> _root{};
    umap<wstring, Shared<UI_Base>> _uiByName{};

    array<Shared<UI_Button>, 3> _characterButtons{};
    array<HeroWardrobeUI, 3> _heroWardrobeUI{};

    PreviewOverrideState _previewOverrideState{};

public:
    static Unique<Wardrobe_Panel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
