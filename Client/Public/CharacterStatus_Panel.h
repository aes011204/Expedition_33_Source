#pragma once

#include "Base.h"
#include "Client_Defines.h"
#include "UIPreviewManager.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
NS_END

NS_BEGIN(Client)
class Player_WorldBase;

// UI_Panel 렌더 객체가 아니라,
// UI_CharacterStatus.json으로 생성된 패널 트리를 제어하는 컨트롤러.
class CharacterStatus_Panel final : public Base
{
public:
    CharacterStatus_Panel() = default;
    ~CharacterStatus_Panel() override = default;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

    void    Open();
    void    Close();
    void    Toggle();

    bool    Is_Open() const { return _isOpen; }

    void    Handle_SelectionInput();
    uint32  Get_WrappedPartyIndex(int32 index) const;

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Refresh_CombatStats();
    void    Refresh_CharacterSelectUI(); // 선택된 캐릭터 슬롯만 크게 보이게

    Shared<UI_Base> Find_UI(const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name) const;

    template<typename T>
    Shared<T> Find_TypedUI(
        const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name) const
    {
        Shared<UI_Base> ui = Find_UI(uiByName, name);
        if (ui == nullptr)
            return nullptr;

        Shared<T> typedUI = dynamic_pointer_cast<T>(ui);
        if (typedUI == nullptr)
            LOG_WARN("CharacterStatusPanel ui type mismatch: {}", String::ToString(name));

        return typedUI;
    }

    static void Set_IntText(const Shared<UI_Text>& text, int32 value);
    static void Set_UIntText(const Shared<UI_Text>& text, uint32 value);
    static void Set_PercentText(const Shared<UI_Text>& text, float value);
    static void Set_HpText(const Shared<UI_Text>& text, float hp, float maxHp);

private:
    vector<UIPreviewManager::PreviewSlotDesc>  Build_PreviewLayout();

private:
    uint32 _selectedHeroIndex = 0;

    uint32 _levelIndex = 0;
    bool _isOpen = false;

    umap<wstring, Shared<UI_Base>> _uiByName{};

    // UI
    Shared<UI_Base> _root{};

    Shared<UI_Text> _levelText{};
    Shared<UI_Text> _statPointText{};

    Shared<UI_Text> _vitalityText{};
    Shared<UI_Text> _mightText{};
    Shared<UI_Text> _agilityText{};
    Shared<UI_Text> _defenseStatText{};
    Shared<UI_Text> _luckText{};

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

public:
    static Unique<CharacterStatus_Panel> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
