#pragma once

#include "Client_Defines.h"

#include <algorithm>
#include <cctype>
#include <string>

NS_BEGIN(Client)

namespace BattleWorldUILevelPlacement
{
    constexpr uint32 kLevelCount = 3;

    inline LevelType Normalize_LevelType(LevelType levelType)
    {
        switch (levelType)
        {
        case LevelType::Stage01:
        case LevelType::GamePlay:
        case LevelType::GestralVillage:
            return levelType;
        default:
            return LevelType::Stage01;
        }
    }

    inline uint32 Resolve_LevelIndex(LevelType levelType)
    {
        switch (Normalize_LevelType(levelType))
        {
        case LevelType::Stage01:
            return 0;
        case LevelType::GamePlay:
            return 1;
        case LevelType::GestralVillage:
            return 2;
        default:
            return 0;
        }
    }

    inline LevelType Get_LevelByIndex(uint32 levelIndex)
    {
        switch (levelIndex)
        {
        case 0:
            return LevelType::Stage01;
        case 1:
            return LevelType::GamePlay;
        case 2:
            return LevelType::GestralVillage;
        default:
            return LevelType::Stage01;
        }
    }

    inline const char* To_LevelJsonName(LevelType levelType)
    {
        switch (Normalize_LevelType(levelType))
        {
        case LevelType::Stage01:
            return "Stage01";
        case LevelType::GamePlay:
            return "GamePlay";
        case LevelType::GestralVillage:
            return "GestralVillage";
        default:
            return "Stage01";
        }
    }

    inline const char* To_LevelEditorLabel(LevelType levelType)
    {
        return To_LevelJsonName(levelType);
    }

    inline bool Try_ParseLevelJsonName(const std::string& levelName, LevelType& outLevelType)
    {
        std::string lowerName = levelName;
        std::transform(
            lowerName.begin(),
            lowerName.end(),
            lowerName.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });

        if (lowerName == "stage01" || lowerName == "stage1")
        {
            outLevelType = LevelType::Stage01;
            return true;
        }

        if (lowerName == "gameplay" || lowerName == "game_play")
        {
            outLevelType = LevelType::GamePlay;
            return true;
        }

        if (lowerName == "gestralvillage" || lowerName == "gestral_village" || lowerName == "gestral village")
        {
            outLevelType = LevelType::GestralVillage;
            return true;
        }

        return false;
    }

}

NS_END
