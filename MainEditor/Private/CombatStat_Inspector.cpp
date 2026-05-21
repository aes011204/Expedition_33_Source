#include "CombatStat_Inspector.h"
#include "CombatStatCom.h"

void CombatStat_Inspector::Draw_CommonRuntimeSection(const Shared<Client::CombatStatCom>& stat)
{
    if (stat == nullptr)
        return;

    Client::CombatStatCom::RuntimeCombatStat& runtime = stat->Get_RuntimeStat();

    float curHp = runtime.CurHp;
    int curShield = static_cast<int>(runtime.CurShield);
    int curAp = static_cast<int>(runtime.CurAp);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Current HP");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat("##CombatCurHp", &curHp, 1.f, 0.f, 999999.f))
    {
        runtime.CurHp = max(0.f, curHp); // 공통 런타임 자원이라 바로 반영
    }

    ImGui::TextDisabled("Current Shield");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragInt("##CombatCurShield", &curShield, 1.f, 0, 999999))
    {
        runtime.CurShield = static_cast<uint32>(max(0, curShield)); // 음수 방지
    }

    ImGui::TextDisabled("Current AP");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragInt("##CombatCurAp", &curAp, 1.f, 0, 999))
    {
        runtime.CurAp = static_cast<uint32>(max(0, curAp)); // 런타임 AP만 직접 수정
    }

    ImGui::TextDisabled("Alive State");
    ImGui::SameLine(120.f);
    ImGui::Text("%d", static_cast<int>(runtime.CurAliveState)); // 지금은 표시만
}
