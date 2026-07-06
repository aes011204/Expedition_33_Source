#pragma once

#include "UI_RuntimePanelBase.h"
#include "Client_Enum.h"
#include "BattleMechanismCom.h"

NS_BEGIN(Engine)
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_BattleSystemHUD final : public UI_RuntimePanelBase
{
public:
    struct PartyBattleStatSlot
    {
        Shared<UI_Base> root{}; // 이 hero portrait 전체를 켜고 끄는 최상위 패널
        Vec2 defaultPosition{}; // JSON 기준 원래 위치

        Shared<UI_Image> hpGauge{};

        Shared<UI_Text> currentHpText{};
        Shared<UI_Text> currentHpTextBg{};
        Shared<UI_Text> maxHpText{};
        Shared<UI_Text> maxHpTextBg{};

        Shared<UI_Text> apText{};
        Shared<UI_Text> apTextBg{};

        Shared<UI_Image> apStates[9]{};

        bool hasBattleStatSnapshot = false;
        int32 cachedCurrentHp = 0;
        int32 cachedMaxHp = 0;
        uint32 cachedCurrentAp = 0;
        uint32 cachedMaxAp = 0;
        float cachedHpRatio = -1.f;
    };

public:
    UI_BattleSystemHUD() = default;
    ~UI_BattleSystemHUD() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

public:
    void    Set_PartyBattleStat(PlayerCharacterType playerType, float curHp, float maxHp, uint32 curAp, uint32 maxAp);
    void    Set_BattleHeroIds(const vector<HeroId>& heroIds);
    void    Set_BattleHeroIds(const HeroId* heroIds, size_t heroCount);
    void    Set_GustaveMechanismCharge(const Shared<BattleMechanismCom>& mechanismCom);
    void    Clear_GustaveMechanismCharge();

    void    Set_LuneMechanismStains(const Shared<BattleMechanismCom>& mechanismCom);
    void    Clear_LuneMechanismStains();

    void    Set_MaelleMechanismStance(const Shared<BattleMechanismCom>& mechanismCom);
    void    Clear_MaelleMechanismStance();

    void    Play_PortraitActivation(HeroId heroId);
    void    Play_PortraitDamaged(HeroId heroId);
    void    Play_PortraitDeactivation(HeroId heroId);
    void    Play_BattleSystemIntro();

    bool    Try_GetPortraitPictosAnchor(HeroId heroId, Vec2& outPosition) const;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    static uint32 Get_SlotIndex(PlayerCharacterType playerType);
    static uint32 Get_SlotIndex(HeroId heroId);
    static void Set_TextPair(const Shared<UI_Text>& front, const Shared<UI_Text>& back, const wstring& text);


private:
    void    Apply_HpGauge(PartyBattleStatSlot& slot, float ratio);
    void    Apply_ApGauge(PartyBattleStatSlot& slot, uint32 slotIndex, uint32 curAp, uint32 maxAp);
    void    Apply_BattlePartyLayout();
    void    Apply_CurrentTurnApTransform(uint32 slotIndex, uint32 apIndex);
    bool    Is_CurrentTurnSlot(uint32 slotIndex) const;
    static void Set_HpText(PartyBattleStatSlot& slot, float hp, float maxHp);
    static void Set_ApText(PartyBattleStatSlot& slot, uint32 ap);
    void    Invalidate_PartyStatSnapshots();

    void    Apply_LuneState(const Shared<BattleMechanismCom>& mechanismCom);
    Shared<UI_Base> Resolve_LuneStainImage(int32 slotIndex, ElementalType element) const;
    void    Close_AllLuneStains();

    void    Apply_MaelleStanceBadge(BattleMechanismCom::MaelleStance stance);
    Shared<UI_Base> Resolve_MaelleStanceBadge(BattleMechanismCom::MaelleStance stance) const;
    void    Close_AllMaelleStanceBadges();

    void    Play_TargetAnimation(const wchar_t* animationName); // Target_Animation은 이름으로 전 UI에 브로드캐스트 재생
    void    Stop_TargetAnimations();  
    void    Capture_TargetAnimationBaseColors();
    void    Restore_TargetAnimationBaseColors();
    void    Reset_TargetAnimationState(bool includeRoot);

private:
    PartyBattleStatSlot _partySlots[3]{};
    array<bool, 3> _visiblePartySlots{ true, true, true }; // 이번 전투에 실제 참여하는 portrait만 연다.
    HeroId _activePortraitHeroId = HeroId::END;
    vector<pair<Shared<UI_Base>, Vec4>> _targetAnimationBaseColors{};

private: /* 구스타브 */
    Shared<UI_Base> _gustaveMechanismRoot{};
    Shared<UI_Text> _gustaveCurrentChargeText{};
    Shared<UI_Text> _gustaveCurrentChargeTextBg{};
    Shared<UI_Text> _gustaveMaxChargeText{};
    Shared<UI_Text> _gustaveMaxChargeTextBg{};
    bool _hasGustaveMechanismSnapshot = false;
    int32 _cachedGustaveCurrentCharge = 0;
    int32 _cachedGustaveMaxCharge = 0;

private: /* 루네 */
    array<array<Shared<UI_Base>, 5>, BattleMechanismCom::kLuneMaxStains> _luneStainImages{};
    array<ElementalType, BattleMechanismCom::kLuneMaxStains> _cachedLuneStains{};
    uint32 _cachedLuneStainCount = 0;
    bool _hasLuneStainSnapshot = false;
    bool _luneStainsClosed = false;

private: /* 마엘 */
    Shared<UI_Base> _maelleCurrentStanceRoot{};
    Shared<UI_Base> _maelleStanceless{};
    Shared<UI_Base> _maelleDefensive{};
    Shared<UI_Base> _maelleOffensive{};
    Shared<UI_Base> _maelleVirtuose{};
    BattleMechanismCom::MaelleStance _cachedMaelleStance = BattleMechanismCom::MaelleStance::Stanceless;
    bool _hasMaelleStanceSnapshot = false;

private:
    static constexpr const wchar_t* kPortraitNames[3] =
    {
        L"Gustave",
        L"Lune",
        L"Maelle",
    };

public:
    static Unique<UI_BattleSystemHUD> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
