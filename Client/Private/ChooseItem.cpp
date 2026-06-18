#include "pch.h"
#include "ChooseItem.h"
#include <Battle_Event.h>
#include "BattleAnimutils.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "Event_Bus.h"

NS_BEGIN(Client)

    ChooseItem::ChooseItem(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

ChooseItem::~ChooseItem()
{
} 

HRESULT ChooseItem::Initialize()
{
    return Battle_State::Initialize();
}

void ChooseItem::Begin_Play()
{
    Battle_State::Begin_Play();


}

bool ChooseItem::EnterInvoke()
{
    auto owner = Get_Owner();
    if (!owner)
        return false;

    owner->Get_TurnActionRequest().itemType = BattleItemType::Invalid; // 메뉴에 다시 들어오면 이전 선택 초기화

    const BattleChar& actor = owner->Get_TurnActionRequest().actor;
    if (actor.ModelCom)
    {
        const uint32 animIndex = actor.ModelCom->Get_AnimationStateIndex();

        if (animIndex != ETOI(BattleAnimType::Item_Enter) &&
            animIndex != ETOI(BattleAnimType::Item_Idle))
        {
            if (!actor.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Item_Enter), 0.2f, false))
            {
                actor.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::Item_Idle), 0.2f, true);
            }
        }
    }

    return Battle_State::EnterInvoke();
}

void ChooseItem::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();
    if (!owner || !battleFSM)
        return;

    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        owner->Get_TurnActionRequest().itemType = BattleItemType::Invalid;

        const BattleChar& actor = owner->Get_TurnAction().Get_CurChar();
        BattleAnimUtils::Play_BattleIdle(actor, 0.3f);

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::PLAYERFOCUS;
        event.subject = owner->Get_TurnActionRequest().actor.actor;
        LOG_INFO("[BattleCamTrace] Publish PLAYERFOCUS from ChooseItem::Update ESC");
        GAME->Get_EventBus().Publish(event);

        battleFSM->Change_State(BattleTurnState::ChooseAction);
        return;
    }

    auto Try_SelectItem = [&](BattleItemType itemType)
    {
        const Item_Data* itemData = CLIENT->Get_Item(itemType);
        if (!itemData)
            return false;

        if (itemData->count <= 0)
            return false; // TODO : 수량 없으면 나중에 Opacity 조절 예정

        owner->Get_TurnActionRequest().itemType = itemData->itemType;
        battleFSM->Change_State(BattleTurnState::ItemTargeting);

        return true;
    };

    if (GAME->KeyDown(KEY_TYPE::Q))
    {
        if (!Try_SelectItem(BattleItemType::HealingTint))
        {
            // 아이템 개수 부족
        }
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::W))
    {
        if (!Try_SelectItem(BattleItemType::EnergyTint))
        {
            // 아이템 개수 부족
        }
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::E))
    {
        if (!Try_SelectItem(BattleItemType::ReviveTint))
        {   
            // 아이템 개수 부족
        }
        return;
    }
}

void ChooseItem::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void ChooseItem::ExitInvoke()
{
    LOG_DEBUG("ChooseItem::ExitInvoke");
    Battle_State::ExitInvoke();
}

void ChooseItem::Free()
{
    Battle_State::Free();
}

Shared<ChooseItem> ChooseItem::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<ChooseItem>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
