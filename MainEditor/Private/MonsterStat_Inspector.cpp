#include "MonsterStat_Inspector.h"

#include "MonsterStatCom.h"

void MonsterStat_Inspector::Draw_Inspector(const Shared<Component>& component)
{
    Shared<MonsterStatCom> stat = dynamic_pointer_cast<MonsterStatCom>(component);
    if (stat == nullptr)
        return;

    if (!Draw_Header("Monster Stat Com"))
        return;

    const auto& desc = stat->Get_MonsterDesc();
    auto& runtime = stat->Get_MonsterRuntimeStat();

    float maxHp = desc.maxHp;
    int maxAp = static_cast<int>(desc.maxAp);
    float physicalAttack = desc.physicalAttack;
    float physicalDefense = desc.physicalDefense;
    float speed = desc.speed;
    bool isBreak = runtime.isBreak;

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
