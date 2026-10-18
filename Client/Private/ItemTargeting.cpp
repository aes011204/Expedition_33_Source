#include "pch.h"
#include "ItemTargeting.h"
#include "BattleAnimutils.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "Battle_Event.h"
#include "CameraPresetTypes.h"
#include "Client_Constants.h"
#include "CombatStatCom.h"
#include "GameInstance.h"
#include "Event_Bus.h"

NS_BEGIN(Client)

ItemTargeting::ItemTargeting(const Shared<BattleController>& owner)
	: Battle_State{ owner }
{
}

ItemTargeting::~ItemTargeting()
{
}

HRESULT ItemTargeting::Initialize()
{
	return Battle_State::Initialize();
}

void ItemTargeting::Begin_Play()
{
	Battle_State::Begin_Play();
}

bool ItemTargeting::EnterInvoke()
{
	auto owner = Get_Owner();
	if (!owner)
		return false;

	_targetCandidates.clear();
	_targetIndex = 0;

	if (!Set_TargetCandidates())
		return false;

	const BattleChar& actor = owner->Get_TurnActionRequest().actor;
	for (uint32 i = 0; i < _targetCandidates.size(); ++i)
	{
		if (_targetCandidates[i].actor == actor.actor)
		{
			_targetIndex = i;
			break;
		}
	}

	owner->Set_TargetPreview(_targetCandidates[_targetIndex]);

	const bool entered = Battle_State::EnterInvoke();
	if (entered)
		GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_ItemTargetAllyStart);

	return entered;
}

void ItemTargeting::Update(float timeDelta)
{
	Battle_State::Update(timeDelta);

	auto owner = Get_Owner();
	auto battleFSM = Get_BattleFSM();

	if (!owner || !battleFSM)
		return;

	const BattleChar& actor = owner->Get_TurnActionRequest().actor;
	if (actor.ModelCom)
	{
		const uint32 animIndex = actor.ModelCom->Get_AnimationStateIndex();
		if (animIndex == ETOI(BattleAnimType::Item_Use))
			return; // 아이템 사용 연출 중에는 대상 변경/취소 입력을 막는다.
	}

	if (GAME->KeyDown(KEY_TYPE::ESCAPE))
	{
		owner->Clear_TargetPreview();
		if (!battleFSM->Change_State(BattleTurnState::ChooseItem))
			return;

		GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_ItemTargetAllyStop);

	    EvtBattlePresentation event;
	    event.cameraMode = BattleCameraMode::SELECTITEM;
	    event.subject = owner->Get_TurnActionRequest().actor.actor;
	    event.presetId = magic_enum::enum_name(owner->Get_TurnActionRequest().actor.heroId);
	    GAME->Get_EventBus().Publish(event);

		return;
	}

	// 왼쪽
	if (GAME->KeyDown(KEY_TYPE::A))
	{
		if (Try_MoveTargetIndex(+1))
		{
			owner->Set_TargetPreview(_targetCandidates[_targetIndex]);
			GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectAlly);

	        EvtBattlePresentation event;
	        event.cameraMode = BattleCameraMode::SELECTPLAYER;
	        event.subject = _targetCandidates[_targetIndex].actor;
	        event.presetId = magic_enum::enum_name(_targetCandidates[_targetIndex].heroId);
	        GAME->Get_EventBus().Publish(event);
		}

		return;
	}
	// 오른쪽
	if (GAME->KeyDown(KEY_TYPE::D))
	{
		if (Try_MoveTargetIndex(-1))
		{
			owner->Set_TargetPreview(_targetCandidates[_targetIndex]);
			GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectAlly);

		    EvtBattlePresentation event;
		    event.cameraMode = BattleCameraMode::SELECTPLAYER;
		    event.subject = _targetCandidates[_targetIndex].actor;
		    event.presetId = magic_enum::enum_name(_targetCandidates[_targetIndex].heroId);
		    GAME->Get_EventBus().Publish(event);
		}
		return;
	}
	// 아이템 사용
	if (GAME->KeyDown(KEY_TYPE::F))
	{
		if (actor.ModelCom == nullptr)
			return;

		const uint32 animIndex = actor.ModelCom->Get_AnimationStateIndex();
		if (animIndex != ETOI(BattleAnimType::Item_Idle))
			return;

		auto& request = owner->Get_TurnActionRequest();
		request.InitTarget = _targetCandidates[_targetIndex];
		GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectAlly);

		if (!actor.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Item_Use), 0.1f, false))
		{
			// 애니메이션 안넣었을 때 fallback
			owner->ExecuteAction(owner->Get_TurnAction());
			BattleAnimUtils::Play_BattleIdle(actor, 0.2f); // fallback에서는 바로 기본 Idle로 복귀한다.
			owner->EndTurn();
			return;
		}
		owner->ExecuteActionRequest(request);

	    EvtBattlePresentation event;
	    event.cameraMode = BattleCameraMode::WIDE;
	    event.subject = owner->Get_TurnActionRequest().actor.actor;
	   //event.presetId = magic_enum::enum_name(owner->Get_TurnActionRequest().actor.heroId);
	    GAME->Get_EventBus().Publish(event);

		return;
	}
}

void ItemTargeting::LateUpdate(float timeDelta)
{
	Battle_State::LateUpdate(timeDelta);
}

void ItemTargeting::ExitInvoke()
{
	Battle_State::ExitInvoke();
}

bool ItemTargeting::Set_TargetCandidates()
{
	auto owner = Get_Owner();
	if (!owner)
		return false;

	_targetCandidates.clear();

	for (const BattleChar& hero : owner->Get_BattleHeroes())
	{
		if (hero.statCom == nullptr)
			continue;

		if (Is_ValidTarget(hero))
		{
			_targetCandidates.push_back(hero);
		}
	}

	return !_targetCandidates.empty();
}

bool ItemTargeting::Try_MoveTargetIndex(int32 direction)
{
	if (_targetCandidates.empty())
		return false;

	int32 nextIndex = static_cast<int32>(_targetIndex) + direction;
	if (nextIndex < 0 || nextIndex >= static_cast<int32>(_targetCandidates.size()))
		return false;

	_targetIndex = static_cast<uint32>(nextIndex);

	return true;
}

bool ItemTargeting::Is_ValidTarget(const BattleChar& target) const
{
	if (target.statCom == nullptr)
		return false;

	auto owner = Get_Owner();
	if (!owner)
		return false;

	const BattleItemType itemType = owner->Get_TurnActionRequest().itemType;

	switch (itemType)
	{
	case BattleItemType::HealingTint:
	case BattleItemType::EnergyTint:    // 살아있는 플레이어만 사용 가능
		return target.statCom->Get_AliveStatus() == EAliveStatus::Alive;

	case BattleItemType::ReviveTint:    // 죽어있는 플레이어한테만 사용 가능
		return target.statCom->Get_AliveStatus() == EAliveStatus::Dead &&
			!target.statCom->Is_ReviveBlocked();

	default:
		return false;
	}

}

void ItemTargeting::Free()
{
	Battle_State::Free();
}

Shared<ItemTargeting> ItemTargeting::Create(const Shared<BattleController>& owner)
{
	auto instance = make_shared<ItemTargeting>(owner);

	if (FAILED(instance->Initialize()))
		return nullptr;

	return instance;
}

NS_END
