#include "BattleQTEController.h"

HRESULT BattleQTEController::Initialize()
{
    Clear();
    return S_OK;
}

void BattleQTEController::Begin(const BattleQTERequest& request)
{
    _activeRequest = request;
    _lastResult = BattleQTEResult::None;
    _hasLastResult = false;
    _isActive = true;

    OnQTEStarted.Broadcast(_activeRequest);
}

bool BattleQTEController::Resolve(BattleQTEResult result)
{
    if (!_isActive)
        return false;

    if (!Is_ValidResult(result))
        return false;

    _isActive = false;
    _lastResult = result;
    _hasLastResult = true;

    OnQTEResolved.Broadcast(result);
    return true;
}

bool BattleQTEController::FailIfActive()
{
    if (!_isActive)
        return false;

    return Resolve(BattleQTEResult::Fail);
}

void BattleQTEController::Clear()
{
    _activeRequest = {};
    _lastResult = BattleQTEResult::None;
    _isActive = false;
    _hasLastResult = false;
}

bool BattleQTEController::Is_ValidResult(BattleQTEResult result) const
{
    return result == BattleQTEResult::Fail ||
        result == BattleQTEResult::SuccessNormal ||
        result == BattleQTEResult::SuccessPerfect;
}

Shared<BattleQTEController> BattleQTEController::Create()
{
    auto instance = make_shared<BattleQTEController>();

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("Failed to Created : BattleQTEController");
        return nullptr;
    }

    return instance;
}

void BattleQTEController::Free()
{
    OnQTEStarted.Clear();
    OnQTEResolved.Clear();

    Clear();
    Base::Free();
}
