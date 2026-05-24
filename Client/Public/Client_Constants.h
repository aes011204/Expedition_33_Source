#pragma once

// 클라 전역에서 공유하는 고정 상수 모음.
namespace Client::Constants::UI
{
    inline constexpr wchar_t Loading_UI_Json_Path[] = L"UI/UI_Json/Scene/Loading/UI_LoadingScean.json";
    inline constexpr wchar_t UI_MainTitle_Path[] = L"UI/UI_Json/Scene/MainTitle/UI_MainTitle.json";
    inline constexpr wchar_t UI_CharacterStatus_Path[] = L"UI/UI_Json/Character/UI_CharacterStatus.json";
    inline constexpr wchar_t UI_SavePoint_Path[] = L"UI/UI_Json/CommonSystem/UI_SavePoint.json";
    inline constexpr wchar_t UI_ExpeditionStatus_Path[] = L"UI/UI_Json/CommonSystem/UI_ExpeditionStatus.json";
    inline constexpr wchar_t UI_ChangeWeapon_Path[] = L"UI/UI_Json/CommonSystem/UI_ChangeWeapon.json";

    inline constexpr wchar_t UI_SkillTree_BG_Path[] = L"UI/UI_Json/Skill/SkillTree/UI_SkillTree_Test.json";
    inline constexpr wchar_t UI_Lune_SkillTree_Path[] = L"UI/UI_Json/Skill/SkillTree/UI_Lune_SkillTree.json";
    inline constexpr wchar_t UI_Gustave_SkillTree_Path[] = L"UI/UI_Json/Skill/SkillTree/UI_Gustave_SkillTree.json";
    inline constexpr wchar_t UI_Maelle_SkillTree_Path[] = L"UI/UI_Json/Skill/SkillTree/UI_Maelle_SkillTree.json";
    inline constexpr wchar_t UI_Lune_SkillTooltips_Dir[] = L"UI/UI_Json/Skill/SkillTooltip/UI_SkillTooltip_Lune.json";
    inline constexpr wchar_t UI_Gustave_SkillTooltips_Dir[] = L"UI/UI_Json/Skill/SkillTooltip/UI_SkillTooltip_Gustave.json";
    inline constexpr wchar_t UI_Maelle_SkillTooltips_Dir[] = L"UI/UI_Json/Skill/SkillTooltip/UI_SkillTooltip_Maelle.json";
    inline constexpr wchar_t UI_Lune_CharacterSheet_Dir[] = L"UI/UI_Json/Character/UI_CharacterSheet_Lune.json";
    inline constexpr wchar_t UI_Gustave_CharacterSheet_Dir[] = L"UI/UI_Json/Character/UI_CharacterSheet_Gustave.json";
    inline constexpr wchar_t UI_Maelle_CharacterSheet_Dir[] = L"UI/UI_Json/Character/UI_CharacterSheet_Maelle.json";

#pragma region 배틀 시스템
    inline constexpr wchar_t UI_Battle_Fade_Path[] = L"UI/UI_Json/BattleSystem/UI_Battle_Fade.json";

    inline constexpr wchar_t UI_BattleSystem_HUD_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_HUD.json";
    inline constexpr wchar_t UI_BattleSystem_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem.json";

    inline constexpr wchar_t UI_BattleSystem_TurnOrder_Path[] = L"UI/UI_Json/BattleSystem/TurnOrder/UI_BattleSystem_TurnOrder.json"; // Legacy
    inline constexpr wchar_t UI_BattleSystem_TurnOrder_Frame_Path[] = L"UI/UI_Json/BattleSystem/TurnOrder/UI_BattleSystem_TurnOrder_Frame.json";
    inline constexpr wchar_t UI_BattleSystem_TurnOrder_CurPlayer_Path[] = L"UI/UI_Json/BattleSystem/TurnOrder/UI_BattleSystem_TurnOrder_CurrentTurn_Player.json";
    inline constexpr wchar_t UI_BattleSystem_TurnOrder_CurMonster_Path[] = L"UI/UI_Json/BattleSystem/TurnOrder/UI_BattleSystem_TurnOrder_CurrentTurn_Monster.json";
    inline constexpr wchar_t UI_BattleSystem_TurnOrder_Player_Path[] = L"UI/UI_Json/BattleSystem/TurnOrder/UI_BattleSystem_TurnOrder_Player.json";
    inline constexpr wchar_t UI_BattleSystem_TurnOrder_Monster_Path[] = L"UI/UI_Json/BattleSystem/TurnOrder/UI_BattleSystem_TurnOrder_Monster.json";

    inline constexpr wchar_t UI_Battle_Defense_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_Defense.json";
    inline constexpr wchar_t UI_Cancel_Aim_Path[] = L"UI/UI_Json/BattleSystem/UI_Cancle_Aim.json";
    inline constexpr wchar_t UI_Choice_Target_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_Choice_Target.json";

    inline constexpr wchar_t UI_Monster_Hp_Path[] = L"UI/UI_Json/BattleSystem/UI_Monster_HP_Bar.json";
    inline constexpr wchar_t UI_Target_Monster_Path[] = L"UI/UI_Json/BattleSystem/UI_Target_Monster.json";

    inline constexpr wchar_t UI_CrossAir_Path[] = L"UI/UI_Json/BattleSystem/UI_CrossAir.json";

    inline constexpr wchar_t UI_NormalDamageFont_Path[] = L"UI/UI_Json/BattleSystem/UI_NormalDamageFont.json";
    inline constexpr wchar_t UI_CriticalDamageFont_Path[] = L"UI/UI_Json/BattleSystem/UI_CriticalDamageFont.json";

    inline constexpr wchar_t UI_Escape_SkipTurn_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_Escape_SkipTurn.json";
    inline constexpr wchar_t UI_Attack_Tooltip_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_AttackTooltip.json";

#pragma endregion

}

namespace Client::Constants::UI_Anim
{
    inline constexpr wchar_t UI_LoadingAnim_Path[] = L"UI/UI_Animations/Scene/Loading";

    inline constexpr wchar_t UI_CrossAirAnim_Path[] = L"UI/UI_Animations/BattleSystem/CrossAir/Ready_CrossAir";
    inline constexpr wchar_t UI_NormalDamageFontAnim_Path[] = L"UI/UI_Animations/BattleSystem/DamageFont/NormalDamageFont";
    inline constexpr wchar_t UI_CriticalDamageFontAnim_Path[] = L"UI/UI_Animations/BattleSystem/DamageFont/CriticalDamageFont";
}

namespace Client::Constants::Map
{
    inline constexpr wchar_t SmallLevel_SimonArea_Remake_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/SmallLevel_SimonArea_Mk1.json";
    inline constexpr wchar_t TestMap_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/TestMap.json";
    inline constexpr wchar_t SaveJson_Dir[] = L"../../../Client/Bin/Resources/Map/SaveJson/";

    inline constexpr wchar_t SpringChunk_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2";
    inline constexpr wchar_t SpringChunk_00_03_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_00_03.json";
    inline constexpr wchar_t SpringChunk_00_04_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_00_04.json";

    inline constexpr wchar_t SpringChunk_01_00_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_01_00.json";
    inline constexpr wchar_t SpringChunk_01_01_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_01_01.json";
    inline constexpr wchar_t SpringChunk_01_02_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_01_02.json";
    inline constexpr wchar_t SpringChunk_01_03_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_01_03.json";
    inline constexpr wchar_t SpringChunk_01_04_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_01_04.json";

    inline constexpr wchar_t SpringChunk_02_00_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_02_00.json";
    inline constexpr wchar_t SpringChunk_02_01_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_02_01.json";

    inline constexpr wchar_t SpringChunk_03_00_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_03_00.json";
    inline constexpr wchar_t SpringChunk_03_01_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_03_01.json";

    inline constexpr wchar_t SpringChunk_04_00_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_04_00.json";
    inline constexpr wchar_t SpringChunk_04_01_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_04_01.json";
    inline constexpr wchar_t SpringChunk_04_02_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_04_02.json";
    inline constexpr wchar_t SpringChunk_04_03_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/chunk_04_03.json";
}

namespace Client::Constants::Charcter
{
    inline constexpr int32 scaleOffset = 10000;
}

namespace Client::Constants::Prefab
{
    inline constexpr char PW_Gustave[]  = "PW_Gustave";
    inline constexpr char PW_Lune[]     = "PW_Lune";
    inline constexpr char PW_Maelle[]   = "PW_Maelle";

    inline constexpr char PB_Gustave[]  = "PB_Gustave";
    inline constexpr char PB_Lune[]     = "PB_Lune";
    inline constexpr char PB_Maelle[]   = "PB_Maelle";

    inline constexpr char MW_Lancelier[] = "MW_Lancelier";
    inline constexpr char MB_Lancelier[] = "MB_Lancelier";

    inline constexpr char MW_Portier[] = "MW_Portier";
    inline constexpr char MB_Portier[] = "MB_Portier";

    inline constexpr char MW_Bishop[] = "MW_Bishop";
    inline constexpr char MB_Bishop[] = "MB_Bishop";

    inline constexpr char MW_Volester[] = "MW_Volester";
    inline constexpr char MB_Volester[] = "MB_Volester";
}
