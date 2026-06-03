#pragma once

#include "BattleMechanismCom.h"
#include "UI_RuntimePanelBase.h"
#include "Client_Enum.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_BattleMechanism final : public UI_RuntimePanelBase
{
public:
    UI_BattleMechanism() = default;
    ~UI_BattleMechanism() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

    void    Show_Hero(HeroId heroId);
    void    Sync_BattleChar(const BattleChar& actor);

private:
    HRESULT Load_UI(uint32 levelIndex);

    HRESULT Load_UIFragment(
        const wchar_t* jsonPath,
        const wstring& prototypeTagPrefix,
        umap<wstring, Shared<UI_Base>>& outUIByName) const;

    HRESULT Bind_UI();

    void    Close_AllRoots();

private:
    void    Apply_GustaveState(const Shared<BattleMechanismCom>& mechanismCom);
    void    Apply_MaelleState(const Shared<BattleMechanismCom>& mechanismCom);
    void    Apply_LuneState(const Shared<BattleMechanismCom>& mechanismCom);
    Shared<UI_Base> Resolve_LuneStainImage(int32 slotIndex, ElementalType element) const;

    void    Close_AllMaelleStances();
    void    Close_AllLuneStains();

private:
#pragma region 구스타브
    Shared<UI_Base> _gustaveRoot{};
    Shared<UI_Text> _gustaveCurrentChargeText{};
    Shared<UI_Text> _gustaveMaxChargeText{};

    array<Shared<UI_Base>, 10> _gustaveChargeGlows{};

#pragma endregion

#pragma region 루네
    Shared<UI_Base> _luneRoot{};
    array<array<Shared<UI_Base>, 4>, BattleMechanismCom::kLuneMaxStains> _luneStainImages{};

#pragma endregion

#pragma region 마엘
    Shared<UI_Base> _maelleRoot{};
    Shared<UI_Base> _maelleStanceless{};    // 태세 없음
    Shared<UI_Base> _maelleVirtuose{};      // 명인 태세
    Shared<UI_Base> _maelleDefensive{};     // 방어 태세
    Shared<UI_Base> _maelleOffensive{};     // 공격 태세
#pragma endregion

    HeroId _currentHeroId = HeroId::END;

public:
    static Unique<UI_BattleMechanism> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
