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

    struct QTE_Slot
    {
        umap<wstring, Shared<UI_Base>> uiByName{};

        Shared<UI_Base> root = nullptr;
        Shared<UI_Base> areaLineRoot = nullptr;
        Shared<UI_Base> decoAngleRoot = nullptr;
        Shared<UI_Base> keySpaceRoot = nullptr;
        Shared<UI_Base> cursorProgressRoot = nullptr;
        Shared<UI_Base> normalDecoRoot = nullptr;
        Shared<UI_Base> perfectDecoRoot = nullptr;
        Shared<UI_Base> perfectTextRoot = nullptr;

        QTE_Stage        stage = QTE_Stage::Closed;
        BattleQTERequest request{};
        float            elapsed = 0.f;
        float            resultElapsed = 0.f;
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
    HRESULT Bind_UIReferences(QTE_Slot& slot);

    void    Update_Slot(QTE_Slot& slot, float timeDelta, bool allowInput);
    void    Begin_Progress(QTE_Slot& slot);
    void    Resolve(QTE_Slot& slot, BattleQTEResult result);
    void    Play_ResultAnimation(QTE_Slot& slot, BattleQTEResult result);

    // 프리셋별 QTE 위치 세팅
    void    Apply_PresetPlacement(QTE_Slot& slot, const BattleQTERequest& request);
    void    Reset_VisualState();
    void    Open_AllQTEVisuals(QTE_Slot& slot);
    void    Close_AllQTEVisuals(QTE_Slot& slot);
    void    Close_ResultVisuals(QTE_Slot& slot);
    void    Play_QTEAnimation(QTE_Slot& slot, const wstring& animationName);
    void    Stop_QTEAnimations(QTE_Slot& slot);
    void    Close_Slot(QTE_Slot& slot);
    void    Set_UIOpen(const Shared<UI_Base>& ui, bool isOpen) const;
    int32   Find_FreeSlotIndex() const;
    int32   Find_FirstInputSlotIndex() const;
    void    Remove_InactiveSlotOrder();

private:
    static constexpr uint32 kMaxQTESlots = 4;
    array<QTE_Slot, kMaxQTESlots> _slots{};
    vector<uint32>   _activeSlotOrder{};

    vector<BattleQTEResult> _pendingResults{};

    float           _resultDuration = 0.7f;

public:
    static Unique<UI_QTE> Create(uint32 levelIndex);
    void Free() override;

};

NS_END
