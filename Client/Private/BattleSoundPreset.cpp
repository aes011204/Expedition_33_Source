#include "pch.h"
#include "BattleSoundPreset.h"

#include "Character_BattleBase.h"
#include "CombatStatCom.h"
#include "GameInstance.h"
#include "TransformCom.h"

namespace
{
    constexpr float kSelfTargetingChance = 0.5f;
    constexpr float kGenericItemUseSfxVolume = 0.5f;
    constexpr float kTintItemUseSfxVolume = 0.75f;
    constexpr float kPickupItemSfxVolume = 0.75f;
    constexpr float kPickupItemVoiceVolume = 1.25f;
    constexpr float kDefaultCueSpeed = 1.f;

    const wchar_t* ToCueHeroName(Client::HeroId heroId)
    {
        switch (heroId)
        {
        case Client::HeroId::Gustave:
            return L"Gustave";
        case Client::HeroId::Lune:
            return L"Lune";
        case Client::HeroId::Maelle:
            return L"Maelle";
        default:
            return nullptr;
        }
    }

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

    bool IsAreaTargetType(Client::TargetType targetType)
    {
        return targetType == Client::TargetType::AllEnemies ||
            targetType == Client::TargetType::AllEnemiesJump;
    }

    bool IsValidMonsterRequest(const Client::ActionRequest& request)
    {
        if (request.actor.actor == nullptr || request.actor.statCom == nullptr)
            return false;

        if (request.actor.statCom->Is_Player())
            return false;

        if (request.skillKey.empty())
            return false;

        return request.type == Client::EBattleContext::Skill ||
            request.type == Client::EBattleContext::BaseAttack;
    }

    bool IsAlivePlayer(const Client::BattleChar& actor)
    {
        if (actor.actor == nullptr || actor.statCom == nullptr)
            return false;

        if (!actor.statCom->Is_Player())
            return false;

        if (ToCueHeroName(actor.heroId) == nullptr)
            return false;

        return actor.statCom->Get_AliveStatus() != Client::EAliveStatus::Dead;
    }

    bool IsAreaTargetingSkill(const Client::Skill_Data* selectedSkill)
    {
        if (selectedSkill == nullptr)
            return false;

        for (const Client::TargetType targetType : selectedSkill->targetingType)
        {
            if (IsAreaTargetType(targetType))
                return true;
        }

        return false;
    }

    bool IsValidTurnStartActor(const Client::BattleChar& actor)
    {
        if (!IsAlivePlayer(actor))
            return false;

        return ResolveTurnStartCueId(actor.heroId) != nullptr;
    }

    bool IsValidItemUser(const Client::BattleChar& actor)
    {
        return IsAlivePlayer(actor);
    }

    bool IsValidItemTarget(const Client::BattleChar& target)
    {
        if (target.actor == nullptr || target.statCom == nullptr)
            return false;

        if (!target.statCom->Is_Player())
            return false;

        return ToCueHeroName(target.heroId) != nullptr;
    }

    size_t RollIndex(size_t count)
    {
        if (count <= 1)
            return 0;

        const size_t index = static_cast<size_t>(GAME->Random(0.f, static_cast<float>(count)));
        return min(index, count - 1);
    }

    wstring MakeSelfTargetingCueId(Client::HeroId targetHeroId)
    {
        if (targetHeroId == Client::HeroId::Gustave)
            return {};

        const wchar_t* targetName = ToCueHeroName(targetHeroId);
        if (targetName == nullptr)
            return {};

        wstring cueId = L"Battle_MonsterTarget_SelfTargeting_";
        cueId += targetName;
        return cueId;
    }

    wstring MakeWarningCueId(Client::HeroId speakerHeroId, Client::HeroId targetHeroId)
    {
        const wchar_t* targetName = ToCueHeroName(targetHeroId);
        if (targetName == nullptr)
            return {};

        wstring cueId = L"Battle_MonsterTarget_Warning_";
        if (speakerHeroId == Client::HeroId::END)
        {
            cueId += L"All";
        }
        else
        {
            if (speakerHeroId == targetHeroId)
                return {};

            const wchar_t* speakerName = ToCueHeroName(speakerHeroId);
            if (speakerName == nullptr)
                return {};

            cueId += speakerName;
        }

        cueId += L"_To_";
        cueId += targetName;
        return cueId;
    }

    wstring MakeItemUseCueId(Client::HeroId userHeroId, bool isSelfTarget)
    {
        const wchar_t* userName = ToCueHeroName(userHeroId);
        if (userName == nullptr)
            return {};

        wstring cueId = userName;
        cueId += isSelfTarget ? L"_UseItem_Self_Voice" : L"_UseItem_Ally_Voice";
        return cueId;
    }

    const wchar_t* ResolveItemUseSfxCueId(Client::BattleItemType itemType)
    {
        switch (itemType)
        {
        case Client::BattleItemType::EnergyTint:
            return L"SFX_Use_EnergyTint";
        case Client::BattleItemType::HealingTint:
            return L"SFX_Use_HealTint";
        case Client::BattleItemType::ReviveTint:
            return L"SFX_Use_ReviveTint";
        default:
            return nullptr;
        }
    }

    const wchar_t* ResolveFindTreasureVoiceCueId(Client::HeroId heroId)
    {
        switch (heroId)
        {
        case Client::HeroId::Gustave:
            return L"Gustave_FindTresure_Voice";
        case Client::HeroId::Lune:
            return L"Lune_FindTresure_Voice";
        case Client::HeroId::Maelle:
            return L"Maelle_FindTresure_Voice";
        default:
            return nullptr;
        }
    }

    Client::HeroId RollFindTreasureHeroId()
    {
        static constexpr Client::HeroId kHeroIds[] =
        {
            Client::HeroId::Gustave,
            Client::HeroId::Lune,
            Client::HeroId::Maelle,
        };

        constexpr size_t heroCount = sizeof(kHeroIds) / sizeof(kHeroIds[0]);
        return kHeroIds[RollIndex(heroCount)];
    }

    bool PlayItemUseSfx(Client::BattleItemType itemType)
    {
        bool played = GAME->Play_SoundCue(
            L"SFX_Use_GenericItem",
            nullptr,
            kGenericItemUseSfxVolume,
            kDefaultCueSpeed);

        const wchar_t* itemSfxCueId = ResolveItemUseSfxCueId(itemType);
        if (itemSfxCueId != nullptr)
        {
            played = GAME->Play_SoundCue(
                itemSfxCueId,
                nullptr,
                kTintItemUseSfxVolume,
                kDefaultCueSpeed) || played;
        }

        return played;
    }
}

NS_BEGIN(Client)

BattleSoundPreset::BattleSoundPreset()
{
}

HRESULT BattleSoundPreset::Initialize()
{
    return S_OK;
}

bool BattleSoundPreset::Play_MonsterTargetingVoice(
    const ActionRequest& request,
    const vector<BattleChar>& targetedHeroes,
    const vector<BattleChar>& battleHeroes,
    const Skill_Data* selectedSkill)
{
    if (!IsValidMonsterRequest(request))
        return false;

    const Shared<Character_BattleBase> currentTurnActor = _monsterTargetingTurnActor.lock();
    if (currentTurnActor == nullptr || currentTurnActor != request.actor.actor)
    {
        _monsterTargetingTurnActor = request.actor.actor;
        _monsterTargetingPlayedThisTurn = false;
    }

    if (_monsterTargetingPlayedThisTurn)
        return false;

    const bool played = IsAreaTargetingSkill(selectedSkill)
        ? Try_PlayAreaWarningVoice(targetedHeroes)
        : Try_PlaySingleTargetVoice(request.InitTarget, battleHeroes);

    if (played)
        _monsterTargetingPlayedThisTurn = true;

    return played;
}

bool BattleSoundPreset::Play_TurnStartVoice(const BattleChar& focusedActor)
{
    if (!IsValidTurnStartActor(focusedActor))
        return false;

    const Shared<Character_BattleBase> currentTurnActor = _turnStartActor.lock();
    if (currentTurnActor == nullptr || currentTurnActor != focusedActor.actor)
    {
        _turnStartActor = focusedActor.actor;
        _turnStartPlayedThisTurn = false;
    }

    if (_turnStartPlayedThisTurn)
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

    _turnStartPlayedThisTurn = true;
    return GAME->Play_SoundCue(cueId, soundPositionPtr);
}

bool BattleSoundPreset::Play_ItemUseVoice(const ActionRequest& request) const
{
    if (request.type != EBattleContext::ConsumableItem)
        return false;

    if (request.itemType == BattleItemType::Invalid)
        return false;

    if (!IsValidItemUser(request.actor) || !IsValidItemTarget(request.InitTarget))
        return false;

    const bool isSelfTarget = request.actor.heroId == request.InitTarget.heroId;
    const wstring cueId = MakeItemUseCueId(request.actor.heroId, isSelfTarget);
    if (cueId.empty())
        return false;

    const bool playedVoice = GAME->Play_SoundCue(cueId);
    const bool playedSfx = PlayItemUseSfx(request.itemType);
    return playedVoice || playedSfx;
}

bool BattleSoundPreset::Play_ChromaPickupItemSound() const
{
    return Play_PickupItemSound(L"SFX_Root_Basic");
}

bool BattleSoundPreset::Play_PictosPickupItemSound() const
{
    return Play_PickupItemSound(L"SFX_Root_Lumina");
}

void BattleSoundPreset::Reset_ForTurn(const BattleChar& actor)
{
    _monsterTargetingTurnActor = actor.actor;
    _monsterTargetingPlayedThisTurn = false;

    _turnStartActor = actor.actor;
    _turnStartPlayedThisTurn = false;
}

bool BattleSoundPreset::Play_PickupItemSound(const wchar_t* itemSfxCueId) const
{
    bool played = false;

    if (itemSfxCueId != nullptr)
        played = GAME->Play_SoundCue(
            itemSfxCueId,
            nullptr,
            kPickupItemSfxVolume,
            kDefaultCueSpeed);

    const HeroId selectedHeroId = RollFindTreasureHeroId();
    const wchar_t* voiceCueId = ResolveFindTreasureVoiceCueId(selectedHeroId);
    if (voiceCueId != nullptr)
    {
        played = GAME->Play_SoundCue(
            voiceCueId,
            nullptr,
            kPickupItemVoiceVolume,
            kDefaultCueSpeed) || played;
    }

    return played;
}

bool BattleSoundPreset::Try_PlaySingleTargetVoice(
    const BattleChar& target,
    const vector<BattleChar>& battleHeroes)
{
    if (!IsAlivePlayer(target))
        return false;

    const bool preferSelfTargeting = GAME->Random(0.f, 1.f) < kSelfTargetingChance;
    if (preferSelfTargeting)
    {
        if (Try_PlaySelfTargetingVoice(target))
            return true;

        return Try_PlayWarningVoice(target, battleHeroes);
    }

    if (Try_PlayWarningVoice(target, battleHeroes))
        return true;

    return Try_PlaySelfTargetingVoice(target);
}

bool BattleSoundPreset::Try_PlayAreaWarningVoice(const vector<BattleChar>& targetedHeroes) const
{
    vector<BattleChar> candidates{};

    for (const BattleChar& target : targetedHeroes)
    {
        if (IsAlivePlayer(target))
            candidates.push_back(target);
    }

    if (candidates.empty())
        return false;

    const size_t firstIndex = RollIndex(candidates.size());
    for (size_t offset = 0; offset < candidates.size(); ++offset)
    {
        const size_t index = (firstIndex + offset) % candidates.size();
        const wstring cueId = MakeWarningCueId(HeroId::END, candidates[index].heroId);
        if (!cueId.empty() && GAME->Play_SoundCue(cueId))
            return true;
    }

    return false;
}

bool BattleSoundPreset::Try_PlaySelfTargetingVoice(const BattleChar& target) const
{
    const wstring cueId = MakeSelfTargetingCueId(target.heroId);
    if (cueId.empty())
        return false;

    return GAME->Play_SoundCue(cueId);
}

bool BattleSoundPreset::Try_PlayWarningVoice(
    const BattleChar& target,
    const vector<BattleChar>& battleHeroes) const
{
    vector<wstring> cueIds{};

    for (const BattleChar& speaker : battleHeroes)
    {
        if (!IsAlivePlayer(speaker))
            continue;

        const wstring cueId = MakeWarningCueId(speaker.heroId, target.heroId);
        if (!cueId.empty())
            cueIds.push_back(cueId);
    }

    if (cueIds.size() >= 2)
    {
        const wstring allCueId = MakeWarningCueId(HeroId::END, target.heroId);
        if (!allCueId.empty())
            cueIds.push_back(allCueId);
    }

    if (cueIds.empty())
        return false;

    const size_t firstIndex = RollIndex(cueIds.size());
    for (size_t offset = 0; offset < cueIds.size(); ++offset)
    {
        const size_t index = (firstIndex + offset) % cueIds.size();
        if (GAME->Play_SoundCue(cueIds[index]))
            return true;
    }

    return false;
}

Unique<BattleSoundPreset> BattleSoundPreset::Create()
{
    Unique<BattleSoundPreset> instance = make_unique<BattleSoundPreset>();

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : BattleSoundPreset");
        return nullptr;
    }

    return instance;
}

void BattleSoundPreset::Free()
{
    _monsterTargetingTurnActor.reset();
    _monsterTargetingPlayedThisTurn = false;

    _turnStartActor.reset();
    _turnStartPlayedThisTurn = false;

    __super::Free();
}

NS_END
