#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class UI_QTE : public UI_RuntimePanelBase
{
private:
    // 현재 QTE 상태
    enum class QTE_Stage
    {
        Closed,
        Intro,
        Progress,
        Result,
    };

public:
    UI_QTE() = default;
    ~UI_QTE() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Close() override;

    void    Play_QTE(const BattleQTERequest& request);
    void    Apply_ExternalResult(BattleQTEResult  result);
    bool    Try_ConsumeResult(BattleQTEResult& outResult);

private:
    HRESULT Load_UI(uint32 levelIndex);

    void    Begin_Progress();
    void    Resolve(BattleQTEResult result);
    void    Play_ResultAnimation(BattleQTEResult result);

private:
    QTE_Stage            _stage = QTE_Stage::Closed;
    BattleQTERequest _request{};

    BattleQTEResult _pendingResult = BattleQTEResult::None;
    bool            _hasPendingResult = false;

    float           _elapsed = 0.f;
    float           _resultElapsed = 0.f;
    float           _resultDuration = 0.7f;

public:
    static Unique<UI_QTE> Create(uint32 levelIndex);
    void Free() override;

};

NS_END
