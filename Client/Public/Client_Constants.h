#pragma once

// 클라 전역에서 공유하는 고정 상수 모음.
namespace Client::Constants::UI
{
    inline constexpr wchar_t Loading_UI_Json_Path[] = L"UI/UI_Json/Scene/Loading/UI_LoadingScean.json";
    inline constexpr wchar_t UI_MainTitle_Path[] = L"UI/UI_Json/Scene/MainTitle/UI_MainTitle.json";
    inline constexpr wchar_t UI_GameEntry_Path[] = L"UI/UI_Json/Scene/GameEntry/UI_GameEntryScene.json";
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
    inline constexpr wchar_t UI_BattleSystem_TurnOrder_CurFrame_Path[] = L"UI/UI_Json/BattleSystem/TurnOrder/UI_BattleSystem_TurnOrder_CurrentTurn_Frame.json";
    inline constexpr wchar_t UI_BattleSystem_TurnOrder_CurPlayer_Path[] = L"UI/UI_Json/BattleSystem/TurnOrder/UI_BattleSystem_TurnOrder_CurrentTurn_Player.json";
    inline constexpr wchar_t UI_BattleSystem_TurnOrder_CurMonster_Path[] = L"UI/UI_Json/BattleSystem/TurnOrder/UI_BattleSystem_TurnOrder_CurrentTurn_Monster.json";
    inline constexpr wchar_t UI_BattleSystem_TurnOrder_Player_Path[] = L"UI/UI_Json/BattleSystem/TurnOrder/UI_BattleSystem_TurnOrder_Player.json";
    inline constexpr wchar_t UI_BattleSystem_TurnOrder_Monster_Path[] = L"UI/UI_Json/BattleSystem/TurnOrder/UI_BattleSystem_TurnOrder_Monster.json";

    inline constexpr wchar_t UI_Battle_Defense_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_Defense.json";
    inline constexpr wchar_t UI_Cancel_Aim_Path[] = L"UI/UI_Json/BattleSystem/UI_Cancle_Aim.json";
    inline constexpr wchar_t UI_Choice_Target_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_Choice_Target.json";

    inline constexpr wchar_t UI_Monster_Hp_Path[] = L"UI/UI_Json/BattleSystem/UI_Monster_HP_Bar.json";
    inline constexpr wchar_t UI_Boss_Hp_Path[] = L"UI/UI_Json/BattleSystem/UI_Boss_HP_Bar.json";
    inline constexpr wchar_t UI_Player_Hp_Path[] = L"UI/UI_Json/BattleSystem/UI_Player_HP_Bar.json";

    inline constexpr wchar_t UI_Target_Monster_Path[] = L"UI/UI_Json/BattleSystem/UI_Target_Monster.json";

    inline constexpr wchar_t UI_CrossAir_Path[] = L"UI/UI_Json/BattleSystem/UI_CrossAir.json";

    // 폰트
    inline constexpr wchar_t UI_NormalDamageFont_Path[] = L"UI/UI_Json/BattleSystem/UI_NormalDamageFont.json";
    inline constexpr wchar_t UI_CriticalDamageFont_Path[] = L"UI/UI_Json/BattleSystem/UI_CriticalDamageFont.json";
    inline constexpr wchar_t UI_HealFont_Path[] = L"UI/UI_Json/BattleSystem/UI_HealFont.json";
    inline constexpr wchar_t UI_BattleSystem_Add_AP_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_Add_AP.json";

    inline constexpr wchar_t UI_Escape_SkipTurnBack_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_Escape_SkipTurn_Back.json";
    inline constexpr wchar_t UI_Attack_Tooltip_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_AttackTooltip.json";

    // 플레이어 메커니즘
    inline constexpr wchar_t UI_Gustave_Mechanism_Path[] = L"UI/UI_Json/BattleSystem/Gustave/UI_BattleSystem_Gustave_Mechanism.json";
    inline constexpr wchar_t UI_Lune_Mechanism_Path[] = L"UI/UI_Json/BattleSystem/Lune/UI_BattleSystem_Lune_Mechanism.json";
    inline constexpr wchar_t UI_Maelle_Mechanism_Path[] = L"UI/UI_Json/BattleSystem/Maelle/UI_BattleSystem_Maelle_Mechanism.json";

    // Reaction
    inline constexpr wchar_t UI_Dodge_Path[] = L"UI/UI_Json/BattleSystem/UI_Dodge.json";
    inline constexpr wchar_t UI_Parry_Path[] = L"UI/UI_Json/BattleSystem/UI_Parry.json";

    // 전투 승리
    inline constexpr wchar_t UI_Battle_Victory_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleVictory.json";

    // 아이템
    inline constexpr wchar_t UI_Battle_ItemPanel_Path[] = L"UI/UI_Json/BattleSystem/Item/UI_BattleSystem_Item.json";
    inline constexpr wchar_t UI_Battle_HealingTooltip_Path[] = L"UI/UI_Json/BattleSystem/Item/UI_BattleSystem_ItemTooltip_Healing_Tint.json";
    inline constexpr wchar_t UI_Battle_EnergyTooltip_Path[] = L"UI/UI_Json/BattleSystem/Item/UI_BattleSystem_ItemTooltip_Energy_Tint.json";
    inline constexpr wchar_t UI_Battle_ReviveTooltip_Path[] = L"UI/UI_Json/BattleSystem/Item/UI_BattleSystem_ItemTooltip_Revive_Tint.json";

    // Battle Announce
    inline constexpr wchar_t UI_BattleSystem_Announce_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_Announce.json";
    inline constexpr wchar_t UI_FirstStrike_Path[] = L"UI/UI_Json/BattleSystem/UI_FirstStrike.json";

    // QTE
    inline constexpr wchar_t UI_QTE_Path[] = L"UI/UI_Json/BattleSystem/UI_QTE.json";

    // 징표
    inline constexpr wchar_t UI_MarkEffect_Path[] = L"UI/UI_Json/BattleSystem/StatusEffect/UI_Mark_Effect.json";
    inline constexpr wchar_t UI_Mark_Path[] = L"UI/UI_Json/BattleSystem/StatusEffect/UI_Mark.json";
    inline constexpr wchar_t UI_Shield_Path[] = L"UI/UI_Json/BattleSystem/StatusEffect/UI_Shield.json";
    inline constexpr wchar_t UI_Destroy_Shield_Path[] = L"UI/UI_Json/BattleSystem/StatusEffect/UI_Destroy_Shield.json";

#pragma endregion

}

namespace Client::Constants::UI_Anim
{
    inline constexpr wchar_t UI_LoadingAnim_Path[] = L"UI/UI_Animations/Scene/Loading";

    inline constexpr wchar_t UI_CrossAirReadyAnim_Path[] =
        L"UI/UI_Animations/BattleSystem/CrossAir/Ready_CrossAir";

    inline constexpr wchar_t UI_CrossAirHoverMonsterAnim_Path[] =
        L"UI/UI_Animations/BattleSystem/CrossAir/Hover_Monster_CrossAir";

    inline constexpr wchar_t UI_CrossAirHoverCancelMonsterAnim_Path[] =
        L"UI/UI_Animations/BattleSystem/CrossAir/Hover_Cancel_Monster_CrossAir";
    
    inline constexpr wchar_t UI_NormalDamageFontAnim_Path[] = L"UI/UI_Animations/BattleSystem/DamageFont/NormalDamageFont";
    inline constexpr wchar_t UI_CriticalDamageFontAnim_Path[] = L"UI/UI_Animations/BattleSystem/DamageFont/CriticalDamageFont";
    
    inline constexpr wchar_t UI_HealFontAnim_Path[] = L"UI/UI_Animations/BattleSystem/DamageFont/HealFont";

    inline constexpr wchar_t UI_BattleSystem_AnnounceAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Announce";
    inline constexpr wchar_t UI_FirstStrikeAnim_Path[] = L"UI/UI_Animations/BattleSystem/FirstStrike";

    inline constexpr wchar_t UI_AddAPAnim_Path[] = L"UI/UI_Animations/BattleSystem/Add_AP";
    inline constexpr wchar_t UI_BattleItemIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Item_Intro";
    inline constexpr wchar_t UI_BattleItemLoopAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Item_Loop";
    inline constexpr wchar_t UI_ControlHintIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/Control_Hint_Intro";
    inline constexpr wchar_t UI_ControlHintOutroAnim_Path[] = L"UI/UI_Animations/BattleSystem/Control_Hint_Outro";
    inline constexpr wchar_t UI_BattleCommandIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleCommand_Intro";
    inline constexpr wchar_t UI_BattleCommandLoopAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleCommand_Loop";
    inline constexpr wchar_t UI_BattleSkillGustaveIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Skill_Gustave_Intro";
    inline constexpr wchar_t UI_BattleSkillGustaveLoopAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Skill_Gustave_Loop";
    inline constexpr wchar_t UI_BattleSkillLuneIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Skill_Lune_Intro";
    inline constexpr wchar_t UI_BattleSkillLuneLoopAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Skill_Lune_Loop";
    inline constexpr wchar_t UI_BattleSkillMaelleIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Skill_Maelle_Intro";
    inline constexpr wchar_t UI_BattleSkillMaelleLoopAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Skill_Maelle_Loop";

#pragma region QTE 애니메이션
    inline constexpr wchar_t UI_QTEIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/QTE/QTE_Intro";
    inline constexpr wchar_t UI_QTEProgressAnim_Path[] = L"UI/UI_Animations/BattleSystem/QTE/QTE_Progress";
    inline constexpr wchar_t UI_QTEFailAnim_Path[] = L"UI/UI_Animations/BattleSystem/QTE/QTE_Fail";
    inline constexpr wchar_t UI_QTESuccessNormalAnim_Path[] = L"UI/UI_Animations/BattleSystem/QTE/QTE_Success_Normal";
    inline constexpr wchar_t UI_QTESuccessPerfectAnim_Path[] = L"UI/UI_Animations/BattleSystem/QTE/QTE_Success_Perfect";
#pragma endregion

    inline constexpr wchar_t UI_MarkEffectLoopAnim_Path[] = L"UI/UI_Animations/BattleSystem/Mark_Effect_Loop";
    inline constexpr wchar_t UI_BrokenShieldAnim_Path[] = L"UI/UI_Animations/BattleSystem/Status/Broken_Shield";
    inline constexpr wchar_t UI_StatusIconIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/Status/Status_Icon_Intro.uianim";

    inline constexpr wchar_t UI_BattleTooltipIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Tooltip_Intro";
    inline constexpr wchar_t UI_BattleTooltipOutroAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Tooltip_Outro";

    inline constexpr wchar_t UI_BattleMechanismIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Mechanism_Intro";

    inline constexpr wchar_t UI_BattleSystemIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleSystem_Intro";

    inline constexpr wchar_t UI_BattleVictoryIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleVictory";
    inline constexpr wchar_t UI_BattleVictoryOutroAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattleVictory_Outro";
    inline constexpr wchar_t UI_TurnOrderFrameIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/TurnOrder/TurnOrder_Frame_Intro";
    inline constexpr wchar_t UI_TurnOrderCurrentTurnFramePulseAnim_Path[] = L"UI/UI_Animations/BattleSystem/TurnOrder/TurnOrder_CurrentTurn_Frame_SizeUpDown";
    inline constexpr wchar_t UI_TurnOrderCurrentTurnMonsterPortraitPulseAnim_Path[] = L"UI/UI_Animations/BattleSystem/TurnOrder/TurnOrder_CurrentTurn_Monster_Portrait_SizeUpDown";
    inline constexpr wchar_t UI_TurnOrderCurrentTurnPlayerPortraitPulseAnim_Path[] = L"UI/UI_Animations/BattleSystem/TurnOrder/TurnOrder_CurrentTurn_Player_Portrait_SizeUpDown";
    inline constexpr wchar_t UI_TurnOrderMonsterPortraitIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/TurnOrder/TurnOrder_Monster_Portrait_Intro";
    inline constexpr wchar_t UI_TurnOrderPlayerPortraitIntroAnim_Path[] = L"UI/UI_Animations/BattleSystem/TurnOrder/TurnOrder_Player_Portrait_Intro";

#pragma region Battle System HUD 애니메이션
    inline constexpr wchar_t UI_BattlePortraitActivationGustaveAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattlePortrait/BattlePortrait_Activation_Gustave";
    inline constexpr wchar_t UI_BattlePortraitActivationLuneAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattlePortrait/BattlePortrait_Activation_Lune";
    inline constexpr wchar_t UI_BattlePortraitActivationMaelleAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattlePortrait/BattlePortrait_Activation_Maelle";

    inline constexpr wchar_t UI_BattlePortraitDamagedGustaveAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattlePortrait/BattlePortrait_Damaged_Gustave";
    inline constexpr wchar_t UI_BattlePortraitDamagedLuneAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattlePortrait/BattlePortrait_Damaged_Lune";
    inline constexpr wchar_t UI_BattlePortraitDamagedMaelleAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattlePortrait/BattlePortrait_Damaged_Maelle";

    inline constexpr wchar_t UI_BattlePortraitDeactivationGustaveAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattlePortrait/BattlePortrait_Deactivation_Gustave";
    inline constexpr wchar_t UI_BattlePortraitDeactivationLuneAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattlePortrait/BattlePortrait_Deactivation_Lune";
    inline constexpr wchar_t UI_BattlePortraitDeactivationMaelleAnim_Path[] = L"UI/UI_Animations/BattleSystem/BattlePortrait/BattlePortrait_Deactivation_Maelle";
#pragma endregion

#pragma region Expedition Status 애니메이션
    inline constexpr wchar_t UI_ExpeditionStatusIntro_Anim_Path[] = L"UI/UI_Animations/Expedition_Status/Expedition_Status_Intro";
    inline constexpr wchar_t UI_ExpeditionStatusOutro_Anim_Path[] = L"UI/UI_Animations/Expedition_Status/Expedition_Status_Outro";
#pragma endregion

#pragma region SavePoint 애니메이션
    inline constexpr wchar_t UI_SavePointIntro_Anim_Path[] = L"UI/UI_Animations/SavePoint/SavePoint_Intro";
    inline constexpr wchar_t UI_SavePointOutro_Anim_Path[] = L"UI/UI_Animations/SavePoint/SavePoint_Outro";
#pragma endregion
}

namespace Client::Constants::Map
{
    inline constexpr wchar_t SmallLevel_SimonArea_Remake_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/SmallLevel_SimonArea_Mk1.json";
    inline constexpr wchar_t TestMap_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/TestMap.json";
    inline constexpr wchar_t SaveJson_Dir[] = L"../../../Client/Bin/Resources/Map/SaveJson/";
    inline constexpr wchar_t GamePlay_DefaultAuthoringMap_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/SmallLevel_MF_Zone_01_Mk1.json";

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

    // multiface
    inline constexpr wchar_t SmallLevel_MF_Zone_01_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/SmallLevel_MF_Zone_01_Copy.json";
    inline constexpr wchar_t LandscapeTextures_Dir[] = L"../../../Client/Bin/Resources/Map/LandscapeTextures/";
    inline constexpr wchar_t MF_Zone_Boss_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/MF_Zone_Boss.hmap";
    inline constexpr wchar_t MF_Zone_Main_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/MF_Zone_Main.hmap";
    inline constexpr wchar_t MF_Zone_Stair_1_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/MF_Zone_Stair_1.hmap";
    inline constexpr wchar_t MF_Zone_Stair_2_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/MF_Zone_Stair_2.hmap";
    inline constexpr wchar_t MF_Zone_Stair_3_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/MF_Zone_Stair_3.hmap";

    inline constexpr wchar_t WaterPath[] = L"../../../Client/Bin/Resources/Map/Water/";
}

namespace Client::Constants::Charcter
{
    inline constexpr int32 scaleOffset = 10000;
}

namespace Client::Constants::Prefab
{
    inline constexpr wchar_t Root_Dir[] = L"Data/json/Prefabs";
    inline constexpr wchar_t Player_Dir[] = L"Data/json/Prefabs/Player";
    inline constexpr wchar_t Monster_Dir[] = L"Data/json/Prefabs/Monster";
    inline constexpr wchar_t Weapon_Dir[] = L"Data/json/Prefabs/Weapons";
    inline constexpr wchar_t BattleStartSpawner_Dir[] = L"Data/json/Prefabs/BattleStartSpawner";

#pragma region 플레이어
    inline constexpr char PW_Gustave[]  = "PW_Gustave";
    inline constexpr char PW_Lune[]     = "PW_Lune";
    inline constexpr char PW_Maelle[]   = "PW_Maelle";

    inline constexpr char PB_Gustave[]  = "PB_Gustave";
    inline constexpr char PB_Lune[]     = "PB_Lune";
    inline constexpr char PB_Maelle[]   = "PB_Maelle";
#pragma endregion

#pragma region 몬스터
    inline constexpr char MW_Lancelier[]        = "MW_Lancelier";
    inline constexpr char MB_Lancelier[]        = "MB_Lancelier";

    inline constexpr char MW_Portier[]          = "MW_Portier";
    inline constexpr char MB_Portier[]          = "MB_Portier";

    inline constexpr char MW_Bishop[]           = "MW_Bishop";
    inline constexpr char MB_Bishop[]           = "MB_Bishop";

    inline constexpr char MW_Volester[]         = "MW_Volester";
    inline constexpr char MB_Volester[]         = "MB_Volester";
#pragma endregion

#pragma region 무기
    inline constexpr char Gustave_Lanceram[]    = "WP_Gustave_Lanceram";
    inline constexpr char Gustave_Lanceram_Gun[]    = "WP_Gustave_Lanceram_Gun";
    inline constexpr char Lune_Colim[]          = "WP_Lune_Colim";
    inline constexpr char Maelle_Medalum[]      = "WP_Maelle_Medalum";
#pragma endregion

    inline constexpr char Eye_Target[]      = "ETC/EyeTarget";

}

// 사운드는 헤더를 따로 뺄까;;
namespace Client::Constants::Sound
{
    constexpr const wchar_t* MainTitleBGM = L"MainTitle/MainTitle_BGM.wav";

    constexpr const wchar_t* MainMenuLoading = L"MainTitle/MainMenu_Loading.wav";
    constexpr const wchar_t* MainMenuNewGame = L"MainTitle/MainMenu_NewGame.wav";
    constexpr const wchar_t* MainMenuQuitGame = L"MainTitle/MainMenu_QuitGame.wav";

    constexpr const wchar_t* SavePointIntro = L"UI/RestMenu/SavePoint/SFX_UI_RestMenu_Open_01 [1].wav";
    constexpr const wchar_t* SavePointOutro = L"UI/RestMenu/SavePoint/SFX_UI_RestMenu_Close_01 [1].wav";

    constexpr const wchar_t* GlobalGenericEnter01 = L"UI/Global/SFX_UI_Global_Generic_Enter_01 [1].wav";
    constexpr const wchar_t* GlobalGenericEnter02 = L"UI/Global/SFX_UI_Global_Generic_Enter_02 [1].wav";
    constexpr const wchar_t* GlobalGenericEnter03 = L"UI/Global/SFX_UI_Global_Generic_Enter_03 [1].wav";
    constexpr const wchar_t* GlobalGenericEnter04 = L"UI/Global/SFX_UI_Global_Generic_Enter_04 [1].wav";
    constexpr const wchar_t* GlobalGenericEnter05 = L"UI/Global/SFX_UI_Global_Generic_Enter_05 [1].wav";

    constexpr const wchar_t* GlobalGenericEnterSounds[5] =
    {
        GlobalGenericEnter01,
        GlobalGenericEnter02,
        GlobalGenericEnter03,
        GlobalGenericEnter04,
        GlobalGenericEnter05
    };

    constexpr const wchar_t* AllySelect01 = L"Battle/UI_Battle_Select_Ally_01 [1].wav";
    constexpr const wchar_t* AllySelect02 = L"Battle/UI_Battle_Select_Ally_02 [1].wav";
    constexpr const wchar_t* AllySelect03 = L"Battle/UI_Battle_Select_Ally_03 [1].wav";

    constexpr const wchar_t* AllySelectSounds[3] =
    {
        AllySelect01,
        AllySelect02,
        AllySelect03
    };

    constexpr const wchar_t* SkillTreeNavigate = L"UI/RestMenu/SkillTree/SFX_UI_RestMenu_Skill_Navigate_01 [1].wav";
    constexpr const wchar_t* SkillTreeLearn = L"UI/RestMenu/SkillTree/SFX_UI_RestMenu_Skill_Learn_01 [1].wav";
    constexpr const wchar_t* SkillTreeBlocked = L"UI/RestMenu/SkillTree/SFX_UI_RestMenu_Skill_Blocked_01 [1].wav";
    constexpr const wchar_t* SpringBGM = L"BGM/SpringMeadow/MUS_SpringMeadows_ENV_Nightfall_PartA.wav";
}

namespace Client::Constants::Cinematic
{
    inline constexpr wchar_t BridgeCinematic[]= L"../../../Client/Bin/Resources/Data/json/Cinematics/Bridge.json";  
        
}
