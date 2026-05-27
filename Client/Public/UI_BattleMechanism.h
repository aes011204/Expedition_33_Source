#pragma once

#include "UI_RuntimePanelBase.h"
#include "Client_Enum.h"

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

private:
    HRESULT Load_UI(uint32 levelIndex);

    HRESULT Load_UIFragment(
        const wchar_t* jsonPath,
        const wstring& prototypeTagPrefix,
        umap<wstring, Shared<UI_Base>>& outUIByName) const;

    HRESULT Bind_UI();

    void    Close_AllRoots();
    void    Close_AllMaelleStances();

private:
    Shared<UI_Base> _gustaveRoot{};
    Shared<UI_Base> _luneRoot{};
    Shared<UI_Base> _maelleRoot{};

    Shared<UI_Base> _maelleStanceless{};    // 태세 없음
    Shared<UI_Base> _maelleVirtuose{};      // 명인 태세
    Shared<UI_Base> _maelleDefensive{};     // 방어 태세
    Shared<UI_Base> _maelleOffensive{};     // 공격 태세

    HeroId _currentHeroId = HeroId::END;

public:
    static Unique<UI_BattleMechanism> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
