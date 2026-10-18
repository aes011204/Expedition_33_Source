#include "WeaponUI_Common.h"

#include "UI_Text.h"

namespace
{
    void Set_GradeText(const Shared<Engine::UI_Text>& text, const string& grade)
    {
        if (text)
            text->Set_Text(wstring(grade.begin(), grade.end()));
    }
}

void Client::Apply_WeaponSummary(const WeaponSummaryText& ui, const GameDataTable::WeaponDesc& weapon)
{
    Apply_WeaponSummary(ui, weapon, weapon.level, weapon.power);
}

void Client::Apply_WeaponSummary(
    const WeaponSummaryText& ui,
    const GameDataTable::WeaponDesc& weapon,
    uint32 level,
    uint32 power)
{
    if (ui.weaponNameText)
        ui.weaponNameText->Set_Text(weapon.weaponName);

    if (ui.levelText)
        ui.levelText->Set_Text(to_wstring(level));

    if (ui.powerText)
        ui.powerText->Set_Text(to_wstring(power));
}

void Client::Apply_WeaponUpgradeSummary(
    const WeaponSummaryText& ui,
    const GameDataTable::WeaponDesc& weapon,
    const GameDataTable::WeaponUpgradeDesc& upgrade)
{
    Apply_WeaponSummary(ui, weapon, upgrade.level, upgrade.power);

    Set_GradeText(ui.vitalityGradeText, upgrade.vitalityGrade);
    Set_GradeText(ui.mightGradeText, upgrade.mightGrade);
    Set_GradeText(ui.agilityGradeText, upgrade.agilityGrade);
    Set_GradeText(ui.defenseGradeText, upgrade.defenseGrade);
    Set_GradeText(ui.luckGradeText, upgrade.luckGrade);
}
