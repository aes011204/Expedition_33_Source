#pragma once

#include "UI_Animation.h"
#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class BattleMechanismCom;
class BattleWorldUIPlane;

class UI_BattleCommandHUD final : public UI_RuntimePanelBase
{
public:
    UI_BattleCommandHUD() = default;
    ~UI_BattleCommandHUD() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

public:
    void    Refresh_ForState(BattleTurnState state);
    void    Set_TargetWorldPosition(const Vec3& worldPos);
    void    Set_CurrentPlayerAP(uint32 curAp, uint32 maxAp);

    // 현재 턴 영웅을 받아서, 영웅별로 offset값 조금씩 조절이 가능하게
    void    Set_CurrentHeroId(HeroId heroId);
    HeroId  Get_EditorActiveHeroId() const override { return _currentHeroId; }

    bool    Supports_EditorHeroTestOffset() const override { return true; }
    Vec2    Get_EditorHeroTestOffset(HeroId heroId) const override;
    void    Set_EditorHeroTestOffset(HeroId heroId, const Vec2& offset) override;

private:
    struct CommandSlot
    {
        Shared<UI_Base> root{};
        Shared<UI_Base> frameBG{};
        Shared<UI_Base> frameLeft{};
        Shared<UI_Base> frameRight{};
        Shared<UI_Base> text{};
        Shared<UI_Base> key{};
    };

private:
    static constexpr uint32 kMaxCommandApSegmentCount = 9;

    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    void    Open_AllHUDUIs();
    void    Close_AllHUDUIs();
    void    Stop_AllHUDAnimations();
    bool    Play_HUDAnimation(const wchar_t* animationName);

    void    Apply_DefaultHighlight();
    HRESULT Bind_CurrentPlayerAP();
    void    Apply_CurrentPlayerAPGauge(uint32 curAp, uint32 maxAp);
    void    Set_CurrentPlayerAPText(uint32 curAp);
    HRESULT Bind_CommandSlot(
        const wstring& commandName,
        const wstring& keyName,
        bool requireKey,
        CommandSlot& outSlot);

    void    Set_CommandSlotHighlighted(const CommandSlot& slot, bool highlighted);
    Vec2    Resolve_HeroTestOffset(HeroId heroId) const;

    HRESULT Ready_WorldBackgroundPlane();
    void    Update_WorldBackgroundPlane();
    void    Set_ScreenBackgroundVisible(bool visible);
    Vec3    Resolve_WorldBackgroundPosition() const;

private:
    vector<Shared<UI_Base>> _allHUDUIs{};
    CommandSlot _attackSlot{};
    CommandSlot _skillSlot{};
    CommandSlot _itemSlot{};
    CommandSlot _shotSlot{};

    Shared<UI_Text>  _currentPlayerApText{};
    Shared<UI_Text>  _currentPlayerApTextBg{};
    Shared<UI_Image> _currentPlayerApStates[kMaxCommandApSegmentCount]{};

    Shared<UI_Base>  _screenBackgroundImage{};
    Shared<UI_Base>  _screenBackgroundDeco{};
    Shared<BattleWorldUIPlane> _worldBackgroundPlane{};
    bool _useWorldBackgroundPlane = false;

private:
    Vec2 _normalScale = Vec2(1.f, 1.f);
    Vec2 _highlightScale = Vec2(1.08f, 1.08f);

    Vec3 _targetWorldPos = Vec3::Zero;
    bool _hasTargetWorldPos = false;
    bool _isPlayingIntro = false;
    float _introElapsed = 0.f;
    float _introDuration = 0.f;

    HeroId _currentHeroId = HeroId::END;

    Vec2 _gustaveTestOffset = Vec2(151.f, -104.f);
    Vec2 _luneTestOffset = Vec2(157.f, -60.f);
    Vec2 _maelleTestOffset = Vec2(143.f, -47.f);

public:
    static Unique<UI_BattleCommandHUD> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
