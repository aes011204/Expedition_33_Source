#include "pch.h"
#include "BattleTargetingVoicePreset.h"

#include "CombatStatCom.h"
#include "GameInstance.h"

namespace
{
    constexpr float kSelfTargetingChance = 0.5f;

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
}

NS_BEGIN(Client)

BattleTargetingVoicePreset::BattleTargetingVoicePreset()
{
}

HRESULT BattleTargetingVoicePreset::Initialize()
{
    return S_OK;
}

bool BattleTargetingVoicePreset::Play(
    const ActionRequest& request,
    const vector<BattleChar>& targetedHeroes,
    const vector<BattleChar>& battleHeroes,
    const Skill_Data* selectedSkill)
{
    if (!IsValidMonsterRequest(request))
        return false;

    const Shared<Character_BattleBase> currentTurnActor = _currentTurnActor.lock();
    if (currentTurnActor == nullptr || currentTurnActor != request.actor.actor)
    {
        _currentTurnActor = request.actor.actor;
        _playedThisTurn = false;
    }

    if (_playedThisTurn)
        return false;

    const bool played = IsAreaTargetingSkill(selectedSkill)
        ? Try_PlayAreaWarningVoice(targetedHeroes)
        : Try_PlaySingleTargetVoice(request.InitTarget, battleHeroes);

    if (played)
        _playedThisTurn = true;

    return played;
}

void BattleTargetingVoicePreset::Reset_ForTurn(const BattleChar& actor)
{
    _currentTurnActor = actor.actor;
    _playedThisTurn = false;
}

bool BattleTargetingVoicePreset::Try_PlaySingleTargetVoice(
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

bool BattleTargetingVoicePreset::Try_PlayAreaWarningVoice(const vector<BattleChar>& targetedHeroes) const
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

bool BattleTargetingVoicePreset::Try_PlaySelfTargetingVoice(const BattleChar& target) const
{
    const wstring cueId = MakeSelfTargetingCueId(target.heroId);
    if (cueId.empty())
        return false;

    return GAME->Play_SoundCue(cueId);
}

bool BattleTargetingVoicePreset::Try_PlayWarningVoice(
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

Unique<BattleTargetingVoicePreset> BattleTargetingVoicePreset::Create()
{
    Unique<BattleTargetingVoicePreset> instance = make_unique<BattleTargetingVoicePreset>();

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : BattleTargetingVoicePreset");
        return nullptr;
    }

    return instance;
}

void BattleTargetingVoicePreset::Free()
{
    _currentTurnActor.reset();
    _playedThisTurn = false;

    __super::Free();
}

NS_END
