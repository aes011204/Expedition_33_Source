#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class BattleQTEController final : public Base
{
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

private:
    bool Is_ValidResult(BattleQTEResult result) const;

private:
    BattleQTERequest _activeRequest{};
    BattleQTEResult  _lastResult = BattleQTEResult::None;
    bool             _isActive = false;
    bool             _hasLastResult = false;

public:
    static Shared<BattleQTEController> Create();
    void Free() override;
};

NS_END
