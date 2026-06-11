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
    DECLARE_DELEGATE(FOnAnnounceFinished);

public:
    UI_BattleAnnounce() = default;
    ~UI_BattleAnnounce() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Close() override;

    void    Show_TurnStart(const BattleChar& actor, BattleTurnState state, const Skill_Data* skillData);
    void    Show_Counter(const vector<BattleChar>& counterActors);
    void    Show_WeakPointHit(const BattleChar& target);

    void    Show_BurnTurnStart(const BattleChar& target, bool notifyFinished);
    void    Show_StunTurnSkip(const BattleChar& target, bool notifyFinished);
    void    Show_Text(const wstring& announceText, bool notifyFinished = false);
    bool    Is_Showing() const { return _isShowing; }

public:
    static wstring Build_TurnStartText(
        const BattleChar& actor,
        BattleTurnState state,
        const Skill_Data* skillData);

    static wstring Build_CounterText(const vector<BattleChar>& counterActors);
    static wstring Build_WeakPointHitText(const BattleChar& target);
    static wstring Build_BurnTurnStartText(const BattleChar& target);
    static wstring Build_StunTurnSkipText(const BattleChar& target);

    // 조사 제거
    static wstring Build_PossessiveSubjectName(const BattleChar& target);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    static wstring Resolve_ActorSubjectName(const BattleChar& actor);

public:
    FOnAnnounceFinished OnAnnounceFinished;

private:
    Shared<UI_Text> _announceText{};

    bool  _isShowing = false;  
    float _elapsed = 0.f;
    float _lifeTime = 1.2f;

    bool _notifyFinished = false;

public:
    static Unique<UI_BattleAnnounce> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
