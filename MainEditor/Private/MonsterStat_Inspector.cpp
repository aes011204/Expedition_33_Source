#include "MonsterStat_Inspector.h"

#include "MonsterStatCom.h"

namespace
{
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
}

Shared<MonsterStat_Inspector> MonsterStat_Inspector::Create()
{
    return make_shared<MonsterStat_Inspector>();
}
