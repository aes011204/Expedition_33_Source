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
};

void Apply_WeaponSummary(const WeaponSummaryText& ui, const GameDataTable::WeaponDesc& weapon);

NS_END
