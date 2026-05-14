#pragma once

// 클라 전역에서 공유하는 고정 상수 모음.
namespace Client::Constants::UI
{
    inline constexpr wchar_t UI_Json_Path[] = L"UI/UI_Json/Json_SaveLoad_Test.json";
    inline constexpr wchar_t Loading_UI_Json_Path[] = L"UI/UI_Json/UI_LoadingScean.json";
    inline constexpr wchar_t Loading_UI_Animation_Dir[] = L"UI/UI_Animations";
    inline constexpr wchar_t UI_SkillTree_Path[] = L"UI/UI_Json/UI_SkillTree.json";

    inline constexpr wchar_t UI_CharacterStatus_Path[] = L"UI/UI_Json/UI_CharacterStatus.json";
    inline constexpr wchar_t UI_SkillTooltip_Dir[] = L"UI/UI_Json/Skill_Tooltip_Json/";
}

namespace Client::Constants::Map
{
    inline constexpr wchar_t SmallLevel_SimonArea_Remake_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/SmallLevel_SimonArea_Remake.json";
    inline constexpr wchar_t SaveJson_Dir[] = L"../../../Client/Bin/Resources/Map/SaveJson/";
}

namespace Client::Constants::Charcter
{
    inline constexpr int32 scaleOffset = 10000;
}
