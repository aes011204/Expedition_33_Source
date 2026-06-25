#include "GamePlayUI_SL.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "PlayerStatCom.h"
#include "SL_Base.h"
#include "SL_Maelle.h"
#include "SoulLikeController.h"
#include "UI_BattleVictory.h"
#include "UI_ReactionResult.h"
#include "UI_SL_HUD.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    wstring Resolve_SLVictoryPlayerName(PlayerCharacterType playerType)
    {
        switch (playerType)
        {
        case PlayerCharacterType::Gustave:
            return L"구스타브";
        case PlayerCharacterType::Lune:
            return L"루네";
        case PlayerCharacterType::Maelle:
            return L"마엘";
        default:
            return {};
        }
    }
}

HRESULT GamePlayUI_SL::Initialize(uint32 levelIndex)
{
	 _levelIndex = levelIndex;
    return S_OK;


}

void GamePlayUI_SL::Update(float timeDelta)
{
    Update_SLHud(timeDelta);
    Update_ReactionResult(timeDelta);
    Update_BattleVictory(timeDelta);
}

void GamePlayUI_SL::Handle_EditorStopRuntime()
{
    if (_slHud)
        _slHud->Clear_PlayerHUD();

    if (_reactionResult)
        _reactionResult->Close();

    _isVictoryUiActive = false;
    _isVictoryConfirmPending = false;

    if (_battleVictory)
        _battleVictory->Close_Immediately();
}

void GamePlayUI_SL::Show_Victory()
{
    if (!Ensure_BattleVictory())
        return;

    _isVictoryUiActive = true;
    _isVictoryConfirmPending = false;

    _battleVictory->Set_VictorySummary(Build_VictorySummary());
    _battleVictory->Set_PlayerStatus(Build_VictoryPlayerStats());
    GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_StateEnd);
    _battleVictory->Open();
}

vector<RuntimeUIEntry> GamePlayUI_SL::Get_RuntimeUIEntries() const
{
	vector<RuntimeUIEntry> entries{};

    if (_slHud)
        _slHud->Append_RuntimeUIEntries(entries, "SoulLike", "SLHUD");

    if (_battleVictory)
        _battleVictory->Append_RuntimeUIEntries(entries, "SoulLike", "BattleVictory");

    if (_reactionResult)
        _reactionResult->Append_RuntimeUIEntries(entries, "SoulLike", "ReactionResult");

    return entries;
}

bool GamePlayUI_SL::Ensure_SLHud()
{
    if (_slHud)
        return true;

    _slHud = UI_SL_HUD::Create(_levelIndex);
    return _slHud != nullptr;
}

bool GamePlayUI_SL::Ensure_BattleVictory()
{
    if (_battleVictory)
        return true;

    _battleVictory = UI_BattleVictory::Create(_levelIndex);
    return _battleVictory != nullptr;
}

bool GamePlayUI_SL::Ensure_ReactionResult()
{
    if (_reactionResult)
        return true;

    _reactionResult = UI_ReactionResult::Create(_levelIndex);
    return _reactionResult != nullptr;
}

void GamePlayUI_SL::Show_ParryResult(const Vec3& worldPos)
{
    UNREFERENCED_PARAMETER(worldPos);

    if (!Ensure_ReactionResult())
        return;

    _reactionResult->Show_Reaction(BattleReactionType::Parry);
}

void GamePlayUI_SL::Show_BossDamage(float damage)
{
    if (!Ensure_SLHud())
        return;

    _slHud->Show_BossDamage(damage);
}

void GamePlayUI_SL::Update_SLHud(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!CLIENT->Is_SoulLikeBattleActive())
    {
        if (_slHud)
            _slHud->Clear_PlayerHUD();
        return;
    }

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    const Shared<SL_Maelle> player = controller ? controller->Get_Player() : nullptr;
    const Shared<SL_Base> monster = controller ? controller->Get_Monster() : nullptr;

    if (player == nullptr)
    {
        if (_slHud)
            _slHud->Clear_PlayerHUD();
        return;
    }

    if (!Ensure_SLHud())
        return;

    _slHud->Update_PlayerHUD(player);
    _slHud->Set_EstherGaugeRatio(controller ? controller->Get_EstherGaugeRatio() : 0.f);
    _slHud->Update_BossHUD(monster, timeDelta);
}

void GamePlayUI_SL::Update_ReactionResult(float timeDelta)
{
    if (_reactionResult == nullptr)
        return;

    if (!CLIENT->Is_SoulLikeBattleActive())
    {
        _reactionResult->Close();
        return;
    }

    _reactionResult->Update(timeDelta);
}

void GamePlayUI_SL::Update_BattleVictory(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (_battleVictory == nullptr)
        return;

    const float uiTimeDelta = GAME->Get_UnscaledTimeDelta();

    if (_isVictoryUiActive)
    {
        _battleVictory->Open();
        _battleVictory->Update(uiTimeDelta);

        if (!_isVictoryConfirmPending && GAME->KeyDown(KEY_TYPE::F))
        {
            const bool requested = CLIENT->Request_ScreenFadeTransition(
                [this]()
                {
                    _isVictoryUiActive = false;
                    _isVictoryConfirmPending = false;

                    if (_battleVictory)
                        _battleVictory->Close_Immediately();

                    if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
                        controller->Confirm_Victory();
                });

            if (requested)
                _isVictoryConfirmPending = true;
        }

        return;
    }

    _battleVictory->Close();
    _battleVictory->Update(uiTimeDelta);

    if (_isVictoryConfirmPending &&
        !_battleVictory->Is_Open() &&
        !_battleVictory->Is_ClosingWithOutro())
    {
        _isVictoryConfirmPending = false;

        if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
            controller->Confirm_Victory();
    }
}

vector<UI_BattleVictory::VictoryPlayerStatDesc> GamePlayUI_SL::Build_VictoryPlayerStats() const
{
    vector<UI_BattleVictory::VictoryPlayerStatDesc> victoryStats{};

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    const Shared<SL_Maelle> player = controller ? controller->Get_Player() : nullptr;
    const Shared<PlayerStatCom> playerStat = player
        ? dynamic_pointer_cast<PlayerStatCom>(player->Get_Stat())
        : nullptr;

    if (playerStat == nullptr)
        return victoryStats;

    const PlayerStatCom::PRuntimeCombatStat& runtimeStat =
        playerStat->Get_PlayerRuntimeStat();
    const uint32 rewardExp = controller ? controller->Get_LastVictoryRewardExp() : 0;

    UI_BattleVictory::VictoryPlayerStatDesc stat{};
    stat.playerType = playerStat->Get_PlayerType();
    stat.visualSlotIndex = 1u;
    stat.portraitGuid = playerStat->Get_StatInfo().portraitGuid;
    stat.displayName = Resolve_SLVictoryPlayerName(stat.playerType);
    stat.level = runtimeStat.CurLevel;
    stat.curExp = runtimeStat.CurExp;
    stat.maxExp = runtimeStat.CurMaxExp > 0 ? runtimeStat.CurMaxExp : max(1u, runtimeStat.nextExp);
    stat.prevLevel = stat.level;
    stat.prevExp = stat.curExp;
    stat.prevMaxExp = stat.maxExp;

    if (rewardExp > 0)
    {
        uint32 rewindLevel = stat.level;
        uint32 rewindExp = stat.curExp;
        uint32 remainingReward = rewardExp;

        while (remainingReward > 0)
        {
            if (remainingReward <= rewindExp)
            {
                rewindExp -= remainingReward;
                remainingReward = 0;
                break;
            }

            remainingReward -= rewindExp;

            if (rewindLevel <= 1)
            {
                rewindExp = 0;
                remainingReward = 0;
                break;
            }

            --rewindLevel;
            rewindExp = CLIENT->Get_NextExp(rewindLevel);
        }

        stat.prevLevel = rewindLevel;
        stat.prevExp = rewindExp;
        stat.prevMaxExp = CLIENT->Get_NextExp(rewindLevel);
    }

    victoryStats.push_back(stat);

    return victoryStats;
}

UI_BattleVictory::VictorySummaryDesc GamePlayUI_SL::Build_VictorySummary() const
{
    UI_BattleVictory::VictorySummaryDesc summary{};

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    const Shared<SL_Base> monster = controller ? controller->Get_Monster() : nullptr;
    const Shared<CombatStatCom> monsterStat = monster ? monster->Get_Stat() : nullptr;

    summary.rewardExp = controller ? controller->Get_LastVictoryRewardExp() : 0;

    if (monsterStat != nullptr)
    {
        const string& portraitGuid = monsterStat->Get_StatInfo().portraitGuid;
        if (!portraitGuid.empty())
            summary.defeatedMonsterPortraitGuids.push_back(portraitGuid);
    }

    return summary;
}

Unique<GamePlayUI_SL> GamePlayUI_SL::Create(uint32 levelIndex)
{
	auto instance = make_unique<GamePlayUI_SL>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : GamePlayUI_SL");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void GamePlayUI_SL::Free()
{
    if (_slHud)
    {
        _slHud->Free();
        _slHud.reset();
    }

    if (_battleVictory)
    {
        _battleVictory->Free();
        _battleVictory.reset();
    }

    if (_reactionResult)
    {
        _reactionResult->Free();
        _reactionResult.reset();
    }

    Base::Free();
}
