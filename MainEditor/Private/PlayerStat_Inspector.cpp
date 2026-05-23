#include "PlayerStat_Inspector.h"
#include "PlayerStatCom.h"

void PlayerStat_Inspector::Draw_Inspector(const Shared<Component>& component)
{
    Shared<PlayerStatCom> stat = dynamic_pointer_cast<PlayerStatCom>(component);
    if (stat == nullptr)
        return;

    if (!Draw_Header("PlayerCombat Stat"))
        return;

    auto& runtime = stat->Get_PlayerRuntimeStat();

    int level = static_cast<int>(runtime.CurLevel);
    int exp = static_cast<int>(runtime.CurExp);
    int statPoint = static_cast<int>(runtime.CurStatPoint);
    int skillPoint = static_cast<int>(runtime.CurSkillPoint);
    int respecCount = static_cast<int>(runtime.CurRespecCount);

    int vitality = runtime.CurVitality;
    int might = runtime.CurMight;
    int agility = runtime.CurAgility;
    int defense = runtime.CurDefense;
    int luck = runtime.CurLuck;

    ImGui::Spacing();

    ImGui::TextDisabled("Level");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool levelChanged = ImGui::DragInt("##PlayerLevel", &level, 1.f, 1, 99);

    ImGui::TextDisabled("Exp");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool expChanged = ImGui::DragInt("##PlayerExp", &exp, 1.f, 0, 999999);

    ImGui::TextDisabled("Stat Point");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool statPointChanged = ImGui::DragInt("##PlayerStatPoint", &statPoint, 1.f, 0, 999);

    ImGui::TextDisabled("Skill Point");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool skillPointChanged = ImGui::DragInt("##PlayerSkillPoint", &skillPoint, 1.f, 0, 999);

    ImGui::TextDisabled("Respec Count");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool respecChanged = ImGui::DragInt("##PlayerRespecCount", &respecCount, 1.f, 0, 99);

    if (levelChanged || expChanged || statPointChanged || skillPointChanged || respecChanged)
    {
        stat->Set_RuntimeProgress(
            static_cast<uint32>(level),
            static_cast<uint32>(exp),
            static_cast<uint32>(statPoint),
            static_cast<uint32>(skillPoint),
            static_cast<uint32>(respecCount));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Vitality");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool vitalityChanged = ImGui::DragInt("##PlayerVitality", &vitality, 1.f, 0, 999);

    ImGui::TextDisabled("Might");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool mightChanged = ImGui::DragInt("##PlayerMight", &might, 1.f, 0, 999);

    ImGui::TextDisabled("Agility");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool agilityChanged = ImGui::DragInt("##PlayerAgility", &agility, 1.f, 0, 999);

    ImGui::TextDisabled("Defense");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool defenseChanged = ImGui::DragInt("##PlayerDefense", &defense, 1.f, 0, 999);

    ImGui::TextDisabled("Luck");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool luckChanged = ImGui::DragInt("##PlayerLuck", &luck, 1.f, 0, 999);

    if (vitalityChanged || mightChanged || agilityChanged || defenseChanged || luckChanged)
    {
        stat->Set_RuntimePrimaryStats(vitality, might, agility, defense, luck);
    }

    // HP / Shield / AP는 파생값 아래가 아니라 "실제 현재 자원"이라 공통 섹션으로 묶는다.
    Draw_CommonRuntimeSection(static_pointer_cast<CombatStatCom>(stat));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Max HP");
    ImGui::SameLine(120.f);
    ImGui::Text("%.1f", runtime.CurMaxHp);

    ImGui::TextDisabled("Attack");
    ImGui::SameLine(120.f);
    ImGui::Text("%.1f", runtime.CurPhysicalAttack);

    ImGui::TextDisabled("Speed");
    ImGui::SameLine(120.f);
    ImGui::Text("%.1f", runtime.CurSpeed);

    ImGui::TextDisabled("Physical Defense");
    ImGui::SameLine(120.f);
    ImGui::Text("%.1f", runtime.CurPhysicalDefense);

    ImGui::TextDisabled("Critical Rate");
    ImGui::SameLine(120.f);
    ImGui::Text("%.1f", runtime.CurCriticalRate);
}

Shared<PlayerStat_Inspector> PlayerStat_Inspector::Create()
{
    return make_shared<PlayerStat_Inspector>();
}
