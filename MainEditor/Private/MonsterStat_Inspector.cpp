#include "MonsterStat_Inspector.h"

#include "MonsterStatCom.h"

namespace
{
    constexpr float kPropertyLabelWidth = 120.f;

    bool DrawEnemyClassCombo(EEnemyClass& enemyClass)
    {
        bool changed = false;
        const string previewLabel = string(magic_enum::enum_name(enemyClass));
        const char* preview = previewLabel.empty() ? "Unknown" : previewLabel.c_str();

        ImGui::TextDisabled("Enemy Class");
        ImGui::SameLine(120.f);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        if (ImGui::BeginCombo("##MonsterEnemyClass", preview))
        {
            for (const auto& [candidate, candidateName] : magic_enum::enum_entries<EEnemyClass>())
            {
                const bool isSelected = (enemyClass == candidate);
                if (ImGui::Selectable(string(candidateName).c_str(), isSelected))
                {
                    enemyClass = candidate; // MonsterStatCom의 PROPERTY_ENUM Enemy Class와 같은 저장값을 편집한다.
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        return changed;
    }

    void ApplyBattleSkillKeys(MonsterStatCom& stat, vector<string>& skillKeys)
    {
        skillKeys.erase(
            remove_if(skillKeys.begin(), skillKeys.end(),
                [](const string& skillKey)
                {
                    return skillKey.empty();
                }),
            skillKeys.end());

        stat.Set_SkillKeys(skillKeys);
        stat.Rebuild_SkillRuntimeState();
    }

    void DrawBattleSkillList(MonsterStatCom& stat)
    {
        vector<string> skillKeys = stat.Get_SkillKeys();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("Battle Skills");

        for (size_t i = 0; i < skillKeys.size(); ++i)
        {
            char buffer[256]{};
            strncpy_s(buffer, skillKeys[i].c_str(), _TRUNCATE);

            ImGui::PushID(static_cast<int>(i));

            ImGui::TextDisabled("Skill %d", static_cast<int>(i + 1));
            ImGui::SameLine(kPropertyLabelWidth);
            ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x - 60.f));
            if (ImGui::InputText("##MonsterBattleSkill", buffer, IM_ARRAYSIZE(buffer)))
            {
                skillKeys[i] = buffer;
                ApplyBattleSkillKeys(stat, skillKeys);
            }

            ImGui::SameLine();
            if (ImGui::Button("X##RemoveMonsterBattleSkill"))
            {
                skillKeys.erase(skillKeys.begin() + i);
                ApplyBattleSkillKeys(stat, skillKeys);
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }

        if (ImGui::Button("Add Skill##MonsterBattleSkill"))
        {
            skillKeys.emplace_back();
            stat.Set_SkillKeys(skillKeys);
            stat.Rebuild_SkillRuntimeState();
        }
    }
}

void MonsterStat_Inspector::Draw_Inspector(const Shared<Component>& component)
{
    Shared<MonsterStatCom> stat = dynamic_pointer_cast<MonsterStatCom>(component);
    if (stat == nullptr)
        return;

    if (!Draw_Header("Monster Stat Com"))
        return;

    auto& desc = stat->Get_MonsterDesc();
    auto& runtime = stat->Get_MonsterRuntimeStat();

    Draw_PortraitTexturePicker(
        "TurnOrder Portrait",
        desc.portraitGuid,
        "textures/ui/battle/turnorder/enemies/");

    Draw_WStringPropertyRow(
        "Battle Announce Name",
        "##MonsterBattleAnnounceName",
        desc.battleAnnounceName);

    int rewardExp = static_cast<int>(desc.exp);
    int rewardChroma = static_cast<int>(desc.chroma);
    int shieldPointsAtSpawn = static_cast<int>(desc.commonCharacterStats.ShieldPointsAtSpawn);


    float maxHp = desc.maxHp;
    int maxAp = static_cast<int>(desc.maxAp);
    float physicalAttack = desc.physicalAttack;
    float physicalDefense = desc.physicalDefense;
    float speed = desc.speed;
    float battleRunMoveSpeed = desc.battleRunMoveSpeed;
    float maxBreak = desc.maxBreak;
    float breakStunDuration = desc.breakBarStunDuration;
    float currentBreak = runtime.curBreak;
    bool isBreak = runtime.isBreak;

    ImGui::Spacing();

    ImGui::TextDisabled("Reward Exp");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragInt("##MonsterRewardExp", &rewardExp, 1.f, 0, 9999999))
    {
        desc.exp = static_cast<uint32>(max(0, rewardExp));
    }

    ImGui::TextDisabled("Reward Chroma");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragInt("##MonsterRewardChroma", &rewardChroma, 1.f, 0, 9999999))
    {
        desc.chroma = static_cast<uint32>(max(0, rewardChroma));
    }

    DrawEnemyClassCombo(desc.enemyClass);

    ImGui::TextDisabled("Shield At Spawn");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragInt("##MonsterShieldPointsAtSpawn", &shieldPointsAtSpawn, 1.f, 0, 99))
    {
        desc.commonCharacterStats.ShieldPointsAtSpawn = static_cast<uint32>(max(0, shieldPointsAtSpawn));
        runtime.CurShield = desc.commonCharacterStats.ShieldPointsAtSpawn;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Max HP");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool maxHpChanged = ImGui::DragFloat("##MonsterMaxHp", &maxHp, 1.f, 0.f, 999999.f);

    ImGui::TextDisabled("Max AP");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool maxApChanged = ImGui::DragInt("##MonsterMaxAp", &maxAp, 1.f, 0, 999);

    ImGui::TextDisabled("Attack");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool attackChanged = ImGui::DragFloat("##MonsterAttack", &physicalAttack, 1.f, 0.f, 999999.f);

    ImGui::TextDisabled("Defense");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool defenseChanged = ImGui::DragFloat("##MonsterDefense", &physicalDefense, 1.f, 0.f, 999999.f);

    ImGui::TextDisabled("Speed");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool speedChanged = ImGui::DragFloat("##MonsterSpeed", &speed, 1.f, 0.f, 999999.f);

    if (maxHpChanged || maxApChanged || attackChanged || defenseChanged || speedChanged)
    {
        stat->Set_RuntimeCombatProfile(
            maxHp,
            static_cast<uint32>(max(0, maxAp)),
            physicalAttack,
            physicalDefense,
            speed);
    }

    ImGui::TextDisabled("Battle Run Speed");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat("##MonsterBattleRunMoveSpeed", &battleRunMoveSpeed, 0.1f, 0.f, 30.f))
    {
        stat->Set_BattleRunMoveSpeed(battleRunMoveSpeed);
    }

    ImGui::TextDisabled("Max Break");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat("##MonsterMaxBreak", &maxBreak, 0.1f, 0.f, 999999.f))
    {
        desc.maxBreak = maxBreak;
        runtime.curBreak = min(runtime.curBreak, desc.maxBreak);
    }

    ImGui::TextDisabled("Break Stun Duration");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat("##MonsterBreakStunDuration", &breakStunDuration, 0.1f, 0.f, 30.f))
    {
        desc.breakBarStunDuration = breakStunDuration;
    }

    ImGui::TextDisabled("Current Break");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat("##MonsterCurrentBreak", &currentBreak, 0.1f, 0.f, max(0.f, desc.maxBreak)))
    {
        stat->Set_Break(currentBreak);
    }

    ImGui::TextDisabled("Break");
    ImGui::SameLine(120.f);
    if (ImGui::Checkbox("##MonsterBreak", &isBreak))
    {
        stat->SetBreak(isBreak); // 몬스터 전용 런타임 플래그
    }

    Draw_CommonRuntimeSection(static_pointer_cast<CombatStatCom>(stat));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Runtime Attack");
    ImGui::SameLine(120.f);
    ImGui::Text("%.1f", runtime.physicalAttack);

    ImGui::TextDisabled("Runtime Defense");
    ImGui::SameLine(120.f);
    ImGui::Text("%.1f", runtime.physicalDefense);

    ImGui::TextDisabled("Runtime Speed");
    ImGui::SameLine(120.f);
    ImGui::Text("%.1f", runtime.CurSpeed);


    ImGui::Spacing();

    ImGui::TextDisabled("criticalRatio");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::DragFloat("##MonsterCriticalRatio", &desc._criticalRatio, 0.1f, 0.f, 1.f);

    ImGui::TextDisabled("midRatio");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::DragFloat("##MonsterMidRatio", &desc._midRatio, 0.1f, 0.f, 1.f);

    DrawBattleSkillList(*stat);

}

Shared<MonsterStat_Inspector> MonsterStat_Inspector::Create()
{
    return make_shared<MonsterStat_Inspector>();
}
