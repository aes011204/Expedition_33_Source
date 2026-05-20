#include "WeaponUI_Common.h"
#include "UI_Text.h"

void Client::Apply_WeaponSummary(const WeaponSummaryText& ui, const GameDataTable::WeaponDesc& weapon)
{
    if (ui.weaponNameText)
        ui.weaponNameText->Set_Text(weapon.weaponName);

    if (ui.levelText)
        ui.levelText->Set_Text(to_wstring(weapon.level));

    if (ui.powerText)
        ui.powerText->Set_Text(to_wstring(weapon.power));
}
