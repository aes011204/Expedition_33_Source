#pragma once

#include "Client_Defines.h"
#include "GameDataTable.h"

NS_BEGIN(Engine)
class UI_Text;
NS_END

NS_BEGIN(Client)

struct WeaponSummaryText
{
    Shared<Engine::UI_Text> weaponNameText{};
    Shared<Engine::UI_Text> levelText{};
    Shared<Engine::UI_Text> powerText{};
    Shared<Engine::UI_Text> vitalityGradeText{};
    Shared<Engine::UI_Text> mightGradeText{};
    Shared<Engine::UI_Text> agilityGradeText{};
    Shared<Engine::UI_Text> defenseGradeText{};
    Shared<Engine::UI_Text> luckGradeText{};
};

void Apply_WeaponSummary(const WeaponSummaryText& ui, const GameDataTable::WeaponDesc& weapon);
void Apply_WeaponSummary(const WeaponSummaryText& ui, const GameDataTable::WeaponDesc& weapon, uint32 level, uint32 power);
void Apply_WeaponUpgradeSummary(
    const WeaponSummaryText& ui,
    const GameDataTable::WeaponDesc& weapon,
    const GameDataTable::WeaponUpgradeDesc& upgrade);

NS_END
