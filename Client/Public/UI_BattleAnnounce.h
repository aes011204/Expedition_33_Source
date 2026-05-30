#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_BattleAnnounce final : public UI_RuntimePanelBase
{
public:
    UI_BattleAnnounce() = default;
    ~UI_BattleAnnounce() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Close() override;

    void    Show_TurnStart(const BattleChar& actor, BattleTurnState state);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    static wstring Resolve_ActorSubjectName(const BattleChar& actor);

    // 상태별 announce 문구를 여기서 만든다.
    static wstring Build_TurnStartText(const BattleChar& actor, BattleTurnState state);

private:
    Shared<UI_Text> _announceText{};

    bool  _isShowing = false;  
    float _elapsed = 0.f;
    float _lifeTime = 1.2f;

public:
    static Unique<UI_BattleAnnounce> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
