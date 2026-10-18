#pragma once

#include "UI_RuntimePanelBase.h"
#include "Client_Defines.h"
#include "UIPreviewManager.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
class UI_Button;
NS_END

NS_BEGIN(Client)
class Player_WorldBase;

// UI_Panel 렌더 객체가 아니라,
// UI_CharacterStatus.json으로 생성된 패널 트리를 제어하는 컨트롤러.
class CharacterStatus_Panel final : public UI_RuntimePanelBase
{
public:
    CharacterStatus_Panel() = default;
    ~CharacterStatus_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;
    void    Toggle() override;

    void    Handle_SelectionInput();
    void    Handle_StatInput();

    uint32  Get_WrappedPartyIndex(int32 index) const;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Refresh_CombatStats();
    void    Refresh_CharacterSelectUI(); // 선택된 캐릭터 슬롯만 크게 보이게

    bool    Adjust_PendingStat(StatType type, int32 value); // 적용 전 스탯 증감
    void    Apply_PendingStats(); // F 저장 시 실제 runtime 반영
    void    Reset_CurrentHeroStats();
    void    Clear_PendingStats();

private:
    static void Set_IntText(const Shared<UI_Text>& text, int32 value);
    static void Set_UIntText(const Shared<UI_Text>& text, uint32 value);
    static void Set_PercentText(const Shared<UI_Text>& text, float value);
    static void Set_HpText(const Shared<UI_Text>& text, float hp, float maxHp);

private:
    vector<UIPreviewManager::PreviewSlotDesc>  Build_PreviewLayout();
    void    Apply_PreviewOverrides();       // UI Preview에서 조명과 포스트프로세스 세팅을 SkillTree_Panel용으로 바꿔준다
    void    Restore_PreviewOverrides();     // UI Preview에서 조명과 포스트프로세스 세팅을 원래대로 돌려준다.
    void    Apply_PreviewRimState();        // UI Preview에서 캐릭터에 역광 효과를 입히는 상태로 만든다. 스킬트리 패널에서만 사용한다.
    void    Clear_PreviewRimState();        // UI Preview에서 캐릭터 역광 효과 상태를 해제한다.
    void    Update_PreviewHeroVisibility();

private:
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
    uint32 _selectedHeroIndex = 0;

    using HeroStatPending = array<int32, static_cast<size_t>(StatType::END)>;
    using PendingStatDelta = array<HeroStatPending, static_cast<size_t>(HeroId::END)>;

    // 플레이어별 임시 분배값. F 누르기 전에는 여기만 바뀜
    PendingStatDelta _pendingStatDelta{};

    // UI
    Shared<UI_Text> _levelText{};
    Shared<UI_Text> _statPointText{};

    Shared<UI_Text> _vitalityText{};
    Shared<UI_Text> _mightText{};
    Shared<UI_Text> _agilityText{};
    Shared<UI_Text> _defenseStatText{};
    Shared<UI_Text> _luckText{};

    Shared<UI_Text> _upgradeVitalityText{};
    Shared<UI_Text> _upgradeMightText{};
    Shared<UI_Text> _upgradeAgilityText{};
    Shared<UI_Text> _upgradeDefenseText{};
    Shared<UI_Text> _upgradeLuckText{};

    Shared<UI_Text> _hpText{};
    Shared<UI_Text> _attackText{};
    Shared<UI_Text> _speedText{};
    Shared<UI_Text> _defenseValueText{};
    Shared<UI_Text> _criticalRateText{};

    Shared<UI_Base> _selectButton01{};
    Shared<UI_Base> _selectButton02{};
    Shared<UI_Base> _selectButton03{};

    Shared<UI_Base> _selectFrame01{};
    Shared<UI_Base> _selectFrame02{};
    Shared<UI_Base> _selectFrame03{};

    Shared<UI_Base> _selectPortrait01{};
    Shared<UI_Base> _selectPortrait02{};
    Shared<UI_Base> _selectPortrait03{};

    Shared<UI_Base> _selectIndicator01{};
    Shared<UI_Base> _selectIndicator02{};
    Shared<UI_Base> _selectIndicator03{};

    Shared<UI_Button> _statusUpgradeButton{};   // F
    Shared<UI_Button> _statusResetButton{};     // R

    // 각 스탯 좌 버튼
    array<Shared<UI_Button>, static_cast<size_t>(StatType::END)> _statDecreaseButtons{};
    // 각 스탯 우 버튼
    array<Shared<UI_Button>, static_cast<size_t>(StatType::END)> _statIncreaseButtons{};
    PreviewOverrideState _previewOverrideState{};

public:
    static Unique<CharacterStatus_Panel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
