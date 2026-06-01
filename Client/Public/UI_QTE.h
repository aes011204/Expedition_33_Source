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
    HRESULT Bind_UIReferences();

    void    Begin_Progress();
    void    Resolve(BattleQTEResult result);
    void    Play_ResultAnimation(BattleQTEResult result);

    // 프리셋별 QTE 위치 세팅
    void    Apply_PresetPlacement(const BattleQTERequest& request);
    void    Reset_VisualState();
    void    Open_AllQTEVisuals();
    void    Close_AllQTEVisuals();
    void    Close_ResultVisuals();
    void    Play_QTEAnimation(const wstring& animationName);
    void    Stop_QTEAnimations();
    void    Set_UIOpen(const Shared<UI_Base>& ui, bool isOpen) const;

private:
    Shared<UI_Base> _areaLineRoot = nullptr;
    Shared<UI_Base> _decoAngleRoot = nullptr;

    // 입력 가능 구간에서만 보이는 Space/Cursor 연출
    Shared<UI_Base> _keySpaceRoot = nullptr;
    Shared<UI_Base> _cursorProgressRoot = nullptr;

    // 결과 전용 연출
    Shared<UI_Base> _normalDecoRoot = nullptr;
    Shared<UI_Base> _perfectDecoRoot = nullptr;
    Shared<UI_Base> _perfectTextRoot = nullptr;

private:
    QTE_Stage        _stage = QTE_Stage::Closed;
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
