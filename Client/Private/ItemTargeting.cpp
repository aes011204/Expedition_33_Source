#include "pch.h"
#include "ItemTargeting.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "CombatStatCom.h"
#include "GameInstance.h"

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

    owner->Set_TargetPreview(_targetCandidates[_targetIndex]);

    return Battle_State::EnterInvoke();
}

void ItemTargeting::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();
    if (!owner || !battleFSM)
        return;

    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        owner->Clear_TargetPreview();
        battleFSM->Change_State(BattleTurnState::ChooseItem);
        return;
    }

    // 왼쪽
    if (GAME->KeyDown(KEY_TYPE::A))
    {
        if (Try_MoveTargetIndex(-1))
        {
            owner->Set_TargetPreview(_targetCandidates[_targetIndex]);
        }
        return;
    }
    // 오른쪽
    if (GAME->KeyDown(KEY_TYPE::D))
    {
        if (Try_MoveTargetIndex(+1))
        {
            owner->Set_TargetPreview(_targetCandidates[_targetIndex]);
        }
        return;
    }
    // 아이템 사용
    if (GAME->KeyDown(KEY_TYPE::F))
    {
        owner->Get_TurnAction().target = _targetCandidates[_targetIndex];
        owner->ExecuteAction(owner->Get_TurnAction());

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

    const Item_Data& itemData = owner->Get_TurnAction()._selected_ItemKey;

    switch (itemData.itemType)
    {
    case BattleItemType::HealingTint:
    case BattleItemType::EnergyTint:    // 살아있는 플레이어만 사용 가능
        return target.statCom->Get_AliveStatus() == EAliveStatus::Alive;

    case BattleItemType::ReviveTint:    // 죽어있는 플레이어한테만 사용 가능
        return target.statCom->Get_AliveStatus() == EAliveStatus::Dead;

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
