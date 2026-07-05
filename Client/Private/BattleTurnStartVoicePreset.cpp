#include "pch.h"
#include "BattleTurnStartVoicePreset.h"

#include "Character_BattleBase.h"
#include "CombatStatCom.h"
#include "GameInstance.h"
#include "TransformCom.h"

namespace
{
    const wchar_t* ResolveTurnStartCueId(Client::HeroId heroId)
    {
        switch (heroId)
        {
        case Client::HeroId::Lune:
            return L"Lune_TurnStart_Voice";
        case Client::HeroId::Maelle:
            return L"Maelle_TurnStart_Voice";
        default:
            return nullptr;
        }
    }

    bool IsValidTurnStartActor(const Client::BattleChar& actor)
    {
        if (actor.actor == nullptr || actor.statCom == nullptr)
            return false;

        if (!actor.statCom->Is_Player())
            return false;

        if (ResolveTurnStartCueId(actor.heroId) == nullptr)
            return false;

        return actor.statCom->Get_AliveStatus() != Client::EAliveStatus::Dead;
    }
}

NS_BEGIN(Client)

BattleTurnStartVoicePreset::BattleTurnStartVoicePreset()
{
}

HRESULT BattleTurnStartVoicePreset::Initialize()
{
    return S_OK;
}

bool BattleTurnStartVoicePreset::Play(const BattleChar& focusedActor)
{
    if (!IsValidTurnStartActor(focusedActor))
        return false;

    const Shared<Character_BattleBase> currentTurnActor = _currentTurnActor.lock();
    if (currentTurnActor == nullptr || currentTurnActor != focusedActor.actor)
    {
        _currentTurnActor = focusedActor.actor;
        _playedThisTurn = false;
    }

    if (_playedThisTurn)
        return false;

    const wchar_t* cueId = ResolveTurnStartCueId(focusedActor.heroId);
    if (cueId == nullptr)
        return false;

    Vec3 soundPosition = Vec3::Zero;
    const Vec3* soundPositionPtr = nullptr;

    if (focusedActor.actor != nullptr && focusedActor.actor->Get_Transform() != nullptr)
    {
        soundPosition = focusedActor.actor->Get_Transform()->Get_WorldPosition();
        soundPositionPtr = &soundPosition;
    }

    _playedThisTurn = true;
    return GAME->Play_SoundCue(cueId, soundPositionPtr);
}

void BattleTurnStartVoicePreset::Reset_ForTurn(const BattleChar& actor)
{
    _currentTurnActor = actor.actor;
    _playedThisTurn = false;
}

Unique<BattleTurnStartVoicePreset> BattleTurnStartVoicePreset::Create()
{
    Unique<BattleTurnStartVoicePreset> instance = make_unique<BattleTurnStartVoicePreset>();

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : BattleTurnStartVoicePreset");
        return nullptr;
    }

    return instance;
}

void BattleTurnStartVoicePreset::Free()
{
    _currentTurnActor.reset();
    _playedThisTurn = false;

    __super::Free();
}

NS_END
