#pragma once

#include "Base.h"
#include "Battle_Enum.h"
#include "Delegate.h"

NS_BEGIN(Client)

class BattleQTEController final : public Base
{
public:
    DECLARE_DELEGATE(
        FOnQTEStarted,
        const BattleQTERequest& /*QTE UI/튜토리얼이 표시 위치, 시간, perfect 구간 */
    );

    DECLARE_DELEGATE(
        FOnQTEResolved,
        BattleQTEResult /*QTE 성공/실패 확정 결과.*/
    );

public:
    BattleQTEController() = default;
    ~BattleQTEController() override = default;

public:
    HRESULT Initialize();

    void Begin(const BattleQTERequest& request);
    bool Resolve(BattleQTEResult result);
    bool FailIfActive();
    void Clear();

    bool Is_Active() const                              { return _isActive; }
    bool Has_LastResult() const                         { return _hasLastResult; }
    BattleQTEResult Get_LastResult() const              { return _lastResult; }
    const BattleQTERequest& Get_ActiveRequest() const   { return _activeRequest; }
    float Consume_NextAttackDamageMultiplier(const BattleChar& attacker, AttackType attackType);

private:
    bool Is_ValidResult(BattleQTEResult result) const;
    bool Is_NextAttackDamageType(AttackType attackType) const;
    float Resolve_DamageBonusRate(BattleQTEResult result) const;
    void Start_Request(const BattleQTERequest& request);

public:
    FOnQTEStarted OnQTEStarted;
    FOnQTEResolved OnQTEResolved;

private:
    BattleQTERequest _activeRequest{};
    BattleQTEResult  _lastResult = BattleQTEResult::None;
    bool             _isActive = false;
    bool             _hasLastResult = false;
    float            _pendingNextAttackDamageBonusRate = 0.f;
    vector<BattleQTERequest> _queuedRequests{};

public:
    static Shared<BattleQTEController> Create();
    void Free() override;
};

NS_END
