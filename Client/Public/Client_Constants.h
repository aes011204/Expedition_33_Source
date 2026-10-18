#pragma once

// 클라 전역에서 공유하는 고정 상수 모음.
namespace Client::Constants::UI
{
    inline constexpr wchar_t Loading_UI_Json_Path[] = L"UI/UI_Json/Scene/Loading/UI_LoadingScean.json";
    inline constexpr wchar_t UI_MainTitle_Path[] = L"UI/UI_Json/Scene/MainTitle/UI_MainTitle.json";
    inline constexpr wchar_t UI_GameEntry_Path[] = L"UI/UI_Json/Scene/GameEntry/UI_GameEntryScene.json";
    inline constexpr wchar_t UI_Level_Announce_Path[] = L"UI/UI_Json/Scene/UI_Level_Announce.json";
    inline constexpr wchar_t UI_CharacterStatus_Path[] = L"UI/UI_Json/Character/UI_CharacterStatus.json";
    inline constexpr wchar_t UI_SavePoint_Path[] = L"UI/UI_Json/CommonSystem/UI_SavePoint.json";
    inline constexpr wchar_t UI_Interaction_Path[] = L"UI/UI_Json/CommonSystem/UI_Interaction.json";
    inline constexpr wchar_t UI_ExpeditionStatus_Path[] = L"UI/UI_Json/CommonSystem/UI_ExpeditionStatus.json";
    inline constexpr wchar_t UI_Journal_Path[] = L"UI/UI_Json/CommonSystem/UI_Journal.json";
    inline constexpr wchar_t UI_ChangeWeapon_Path[] = L"UI/UI_Json/CommonSystem/UI_ChangeWeapon.json";
    inline constexpr wchar_t UI_ChangeWeapon_Current_Weapon_Tooltip_Path[] =
        L"UI/UI_Json/CommonSystem/UI_ChangeWeapon_Current_Weapon_Tooltip.json";
    inline constexpr wchar_t UI_UpgradeWeapon_Path[] = L"UI/UI_Json/CommonSystem/UI_Upgrade_Weapon.json";
    inline constexpr wchar_t UI_Merchant_Path[] = L"UI/UI_Json/CommonSystem/UI_Merchant.json";
    inline constexpr wchar_t UI_Replay_Path[] = L"UI/UI_Json/CommonSystem/UI_RePlay.json";

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
    inline constexpr wchar_t UI_BattleSystem_Gradient_Gauge_Path[] = L"UI/UI_Json/BattleSystem/UI_BattleSystem_Gradient_Gauge.json";

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
    inline constexpr wchar_t UI_BurnIcon_Path[] = L"UI/UI_Json/BattleSystem/StatusEffect/UI_Burn_Icon.json";
    inline constexpr wchar_t UI_Burn_Path[] = L"UI/UI_Json/BattleSystem/StatusEffect/UI_Burn.json";
    inline constexpr wchar_t UI_SlowIcon_Path[] = L"UI/UI_Json/BattleSystem/StatusEffect/UI_Slow_Icon.json";
    inline constexpr wchar_t UI_Slow_Path[] = L"UI/UI_Json/BattleSystem/StatusEffect/UI_Slow.json";

    // Tutorial
    inline constexpr wchar_t UI_Tutorial_BattleSystem_Path[] = L"UI/UI_Json/BattleSystem/Tutorial/UI_Tutorial_BattleSystem.json";
    inline constexpr wchar_t UI_Tutorial_TurnOrder_Path[]    = L"UI/UI_Json/BattleSystem/Tutorial/UI_Tutorial_TurnOrder.json";
    inline constexpr wchar_t UI_Tutorial_Attack_Path[]       = L"UI/UI_Json/BattleSystem/Tutorial/UI_Tutorial_Attack.json";
    inline constexpr wchar_t UI_Tutorial_EnemyTurn_Path[]    = L"UI/UI_Json/BattleSystem/Tutorial/UI_Tutorial_MonsterTurn.json";
    inline constexpr wchar_t UI_Tutorial_Dodge_Path[]        = L"UI/UI_Json/BattleSystem/Tutorial/UI_Tutorial_Dodge.json";
    inline constexpr wchar_t UI_Tutorial_Skill_Path[]        = L"UI/UI_Json/BattleSystem/Tutorial/UI_Tutorial_Skill.json";
    inline constexpr wchar_t UI_Tutorial_QTE_Path[]          = L"UI/UI_Json/BattleSystem/Tutorial/UI_Tutorial_QTE.json";
    inline constexpr wchar_t UI_Tutorial_Counter_Path[]      = L"UI/UI_Json/BattleSystem/Tutorial/UI_Tutorial_Counter.json";
    inline constexpr wchar_t UI_Tutorial_FreeAim_Path[]      = L"UI/UI_Json/BattleSystem/Tutorial/UI_Tutorial_FreeAim.json";
    inline constexpr wchar_t UI_Tutorial_Gradient_Path[]     = L"UI/UI_Json/BattleSystem/Tutorial/UI_Tutorial_Gradient.json";

#pragma endregion

#pragma region 플레이어 스테이터스. 탭눌렀을 때
    inline constexpr wchar_t UI_Character_Progression_Path[] =
        L"UI/UI_Json/CommonSystem/UI_Character_Progression.json";

    inline constexpr wchar_t UI_ChangeSkill_Path[] =
        L"UI/UI_Json/CommonSystem/UI_ChangeSkill.json";

    inline constexpr wchar_t UI_Available_Skill_Path[] =
        L"UI/UI_Json/CommonSystem/UI_Available_Skill.json";

    inline constexpr wchar_t UI_Wardrobe_Path[] =
        L"UI/UI_Json/CommonSystem/UI_Wardrobe.json";

    inline constexpr wchar_t UI_Pictos_Path[] =
        L"UI/UI_Json/CommonSystem/UI_Pictos.json";
#pragma endregion

#pragma region 해금 시스템
    inline constexpr wchar_t UI_Get_Character_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/UI_Get_Character.json";

    inline constexpr wchar_t UI_Get_Clothes_Baguette_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Clothes/UI_Get_Clothes_Baguette.json";

    inline constexpr wchar_t UI_Get_Clothes_PotatoBag_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Clothes/UI_Get_Clothes_PotatoBag.json";

    inline constexpr wchar_t UI_Get_Weapon_Gustave_Baguette_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Weapon/UI_Get_Weapon_Gustave_Baguette.json";

    inline constexpr wchar_t UI_Get_Weapon_Gustave_Candy_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Weapon/UI_Get_Weapon_Gustave_Candy.json";

    inline constexpr wchar_t UI_Get_Weapon_Gustave_Lanceram_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Weapon/UI_Get_Weapon_Gustave_Lanceram.json";

    inline constexpr wchar_t UI_Get_Weapon_Lune_Baguette_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Weapon/UI_Get_Weapon_Lune_Baguette.json";

    inline constexpr wchar_t UI_Get_Weapon_Lune_Colim_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Weapon/UI_Get_Weapon_Lune_Colim.json";

    inline constexpr wchar_t UI_Get_Weapon_Lune_Snowim_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Weapon/UI_Get_Weapon_Lune_Snowim.json";

    inline constexpr wchar_t UI_Get_Weapon_Maelle_Baguette_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Weapon/UI_Get_Weapon_Maelle_Baguette.json";

    inline constexpr wchar_t UI_Get_Weapon_Maelle_Medalum_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Weapon/UI_Get_Weapon_Maelle_Medalum.json";

    inline constexpr wchar_t UI_Get_Weapon_Maelle_Sekarum_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Weapon/UI_Get_Weapon_Maelle_Sekarum.json";

    inline constexpr wchar_t UI_Get_Chroma_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Chroma/UI_Get_Chroma.json";

    inline constexpr wchar_t UI_Get_Pictos_AugmentedAim_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Pictos/UI_Get_Pictos_AugmentedAim.json";

    inline constexpr wchar_t UI_Get_Pictos_AugmentedAttack_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Pictos/UI_Get_Pictos_AugmentedAttack.json";

    inline constexpr wchar_t UI_Get_Pictos_EnergisingTurn_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Pictos/UI_Get_Pictos_EnergisingTurn.json";

    inline constexpr wchar_t UI_Get_Pictos_GlassCannon_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Pictos/UI_Get_Pictos_GlassCannon.json";

    inline constexpr wchar_t UI_Get_Pictos_Roulette_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Pictos/UI_Get_Pictos_Roulette.json";

    inline constexpr wchar_t UI_Get_Pictos_WarmingUp_Path[] =
        L"UI/UI_Json/CommonSystem/Get_Item/Pictos/UI_Get_Pictos_WarmingUp.json";

#pragma endregion

#pragma region 쌍검투사
    inline constexpr wchar_t UI_Arena_Character_Select_Path[] =
        L"UI/UI_Json/CommonSystem/UI_Arena_Character_Select.json";
#pragma endregion

#pragma region 배틀 픽토스
    inline constexpr wchar_t UI_Battle_Activation_Pictos_Path[] =
        L"UI/UI_Json/BattleSystem/UI_Battle_Activation_Pictos.json";
#pragma endregion

#pragma region 배틀 점프 리액션
    inline constexpr wchar_t UI_Jump_Path[] =
        L"UI/UI_Json/BattleSystem/UI_Jump.json";

    inline constexpr wchar_t UI_JumpCounter_Path[] =
        L"UI/UI_Json/BattleSystem/UI_JumpCounter.json";
#pragma endregion

#pragma region SoulLike
    inline constexpr wchar_t UI_SL_HUD_Path[] = L"UI/UI_Json/Soulslike/UI_SL_HUD.json";
    inline constexpr wchar_t UI_SL_LockOn_Path[] = L"UI/UI_Json/Soulslike/UI_SL_LockOn.json";
#pragma endregion

#pragma region Dialogue
    inline constexpr wchar_t UI_Dialogue[] = L"UI/UI_Json/CommonSystem/UI_Dialogue.json";
    inline constexpr wchar_t UI_WorldDialogue[] = L"UI/UI_Json/CommonSystem/UI_WorldDialogue.json";
    inline constexpr wchar_t UI_WorldDialogue_GestralVillage[] =
        L"UI/UI_Json/CommonSystem/UI_WorldDialogue_GestralVillage.json";
    inline constexpr wchar_t UI_WorldDialogue_GestralVillage_Second[] =
        L"UI/UI_Json/CommonSystem/UI_WorldDialogue_GestralVillage_Second.json";
    inline constexpr wchar_t UI_WorldDialogue_Simon[] =
        L"UI/UI_Json/CommonSystem/UI_WorldDialogue_Simon.json";
#pragma endregion

}

namespace Client::Constants::UI_Anim
{
    inline constexpr wchar_t UI_LoadingAnim_Path[] = L"UI/UI_Animations/Scene/Loading";

    inline constexpr wchar_t UI_CharacterSelectLuneAnim_Path[] =
        L"UI/UI_Animations/Character_Select/CharacterSelect_Lune";
    inline constexpr wchar_t UI_CharacterSelectGustaveAnim_Path[] =
        L"UI/UI_Animations/Character_Select/CharacterSelect_Gustave";
    inline constexpr wchar_t UI_CharacterSelectMaelleAnim_Path[] =
        L"UI/UI_Animations/Character_Select/CharacterSelect_Maelle";

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
    inline constexpr wchar_t UI_LevelAnnounceIntroAnim_Path[] = L"UI/UI_Animations/Scene/Level_Announce_Intro";
    inline constexpr wchar_t UI_InteractionIntroAnim_Path[] = L"UI/UI_Animations/Common/Interaction_Intro";
    inline constexpr wchar_t UI_InteractionOutroAnim_Path[] = L"UI/UI_Animations/Common/Interaction_Outro";
    inline constexpr wchar_t UI_WorldDialogue_GestralVillageAnim_Path[] =
        L"UI/UI_Animations/Common/WorldDialogue_GestralVillage";
    inline constexpr wchar_t UI_WorldDialogue_GestralVillageSecondAnim_Path[] =
        L"UI/UI_Animations/Common/WorldDialogue_GestralVillage_Second";
    inline constexpr wchar_t UI_WorldDialogue_SimonAnim_Path[] =
        L"UI/UI_Animations/Common/WorldDialogue_Simon";
    inline constexpr wchar_t UI_JournalUIIntroAnim_Path[] =
        L"UI/UI_Animations/Common/Journal/Journal_UI_Intro";
    inline constexpr wchar_t UI_JournalMapIntroAnim_Path[] =
        L"UI/UI_Animations/Common/Journal/Journal_Map_Intro";
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

    inline constexpr wchar_t UI_BattleDefenseUseDodgeAnim_Path[] = L"UI/UI_Animations/BattleSystem/Use_Dodge";
    inline constexpr wchar_t UI_BattleDefenseUseParryAnim_Path[] = L"UI/UI_Animations/BattleSystem/Use_Parry";
    inline constexpr wchar_t UI_BattleDefenseUseJumpAnim_Path[] = L"UI/UI_Animations/BattleSystem/Use_Jump";

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

#pragma region Tutorial 애니메이션
    inline constexpr wchar_t UI_Tutorial_BattleSystemAnim_Path[] = L"UI/UI_Animations/Tutorial/Tutorial_BattleSystem";
    inline constexpr wchar_t UI_Tutorial_TurnOrderAnim_Path[]    = L"UI/UI_Animations/Tutorial/Tutorial_TurnOrder";
    inline constexpr wchar_t UI_Tutorial_AttackAnim_Path[]       = L"UI/UI_Animations/Tutorial/Tutorial_Attack";
    inline constexpr wchar_t UI_Tutorial_EnemyTurnAnim_Path[]    = L"UI/UI_Animations/Tutorial/Tutorial_MonsterTurn";
    inline constexpr wchar_t UI_Tutorial_DodgeAnim_Path[]        = L"UI/UI_Animations/Tutorial/Tutorial_Dodge";
    inline constexpr wchar_t UI_Tutorial_SkillAnim_Path[]        = L"UI/UI_Animations/Tutorial/Tutorial_Skill";
    inline constexpr wchar_t UI_Tutorial_QTEAnim_Path[]          = L"UI/UI_Animations/Tutorial/Tutorial_QTE";
    inline constexpr wchar_t UI_Tutorial_CounterAnim_Path[]      = L"UI/UI_Animations/Tutorial/Tutorial_Counter";
    inline constexpr wchar_t UI_Tutorial_FreeAimAnim_Path[]      = L"UI/UI_Animations/Tutorial/Tutorial_FreeAim";
    inline constexpr wchar_t UI_Tutorial_GradientAnim_Path[]     = L"UI/UI_Animations/Tutorial/Tutorial_Gradient";
#pragma endregion

#pragma region 해금 시스템
     inline constexpr wchar_t UI_GetCharacterAnim_Path[] =
        L"UI/UI_Animations/Tutorial/Get_Character";

    inline constexpr wchar_t UI_GetClothesBaguetteIntroAnim_Path[] =
        L"UI/UI_Animations/Common/Get_Clothes_Baguette_Intro";

    inline constexpr wchar_t UI_GetClothesBaguetteOutroAnim_Path[] =
        L"UI/UI_Animations/Common/Get_Clothes_Baguette_Outro";

    inline constexpr wchar_t UI_GetClothesPotatoBagIntroAnim_Path[] =
        L"UI/UI_Animations/Common/Get_Clothes_PotatoBag_Intro";

    inline constexpr wchar_t UI_GetClothesPotatoBagOutroAnim_Path[] =
        L"UI/UI_Animations/Common/Get_Clothes_PotatoBag_Outro";

    inline constexpr wchar_t UI_GetItemAnim_Path[] =
        L"UI/UI_Animations/Common/Get_Item";
#pragma endregion

#pragma region 배틀 픽토스
     inline constexpr wchar_t UI_BattlePictosActivationAnim_Path[] =
        L"UI/UI_Animations/BattleSystem/Battle_Pictos_Activation";

inline constexpr wchar_t UI_BattlePictosDeactivationAnim_Path[] =
        L"UI/UI_Animations/BattleSystem/Battle_Pictos_Deactivation";
#pragma endregion

#pragma region 배틀 점프 리액션
    inline constexpr wchar_t UI_JumpTextIntroAnim_Path[] =
        L"UI/UI_Animations/BattleSystem/Jump_Text_Intro";

    inline constexpr wchar_t UI_JumpCounterIntroAnim_Path[] =
        L"UI/UI_Animations/BattleSystem/JumpCounter_Intro";

    inline constexpr wchar_t UI_JumpCounterSuccessAnim_Path[] =
        L"UI/UI_Animations/BattleSystem/JumpCounter_Success";
#pragma endregion

#pragma region Dialogue
    inline constexpr wchar_t UI_Choice_Dialogue_First_Hover_Path[] =
        L"UI/UI_Animations/Common/Dialogue/Choice_Dialogue_First_Button_Hover";

    inline constexpr wchar_t UI_Choice_Dialogue_First_Hover_Cancel_Path[] =
        L"UI/UI_Animations/Common/Dialogue/Choice_Dialogue_First_Button_Hover_Cancel";

    inline constexpr wchar_t UI_Choice_Dialogue_Intro[] =
        L"UI/UI_Animations/Common/Dialogue/Choice_Dialogue_Intro";

    inline constexpr wchar_t UI_Choice_Dialogue_Second_Hover_Path[] =
        L"UI/UI_Animations/Common/Dialogue/Choice_Dialogue_Second_Button_Hover";

    inline constexpr wchar_t UI_Choice_Dialogue_Second_Hover_Cancel_Path[] =
        L"UI/UI_Animations/Common/Dialogue/Choice_Dialogue_Second_Button_Hover_Cancel";

    inline constexpr wchar_t UI_Dialogue_Intro[] =
        L"UI/UI_Animations/Common/Dialogue/Dialogue_Intro";
#pragma endregion

}

namespace Client::Constants::Map
{
    inline constexpr wchar_t SmallLevel_SimonArea_Remake_Path[] = L"../../../Client/Bin/Resources/Map/MapInstance/SmallLevel_SimonArea";
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
    //inline constexpr wchar_t SmallLevel_MF_Zone_01_Path[] = L"../../../Client/Bin/Resources/Map/SaveJson/SmallLevel_MF_Zone_01_Copy.json";
    inline constexpr wchar_t SmallLevel_MF_Zone_01_Path[] = L"../../../Client/Bin/Resources/Map/MapInstance/SmallLevel_MF_Zone_01_Mk1";
    inline constexpr wchar_t LandscapeTextures_Dir[] = L"../../../Client/Bin/Resources/Map/LandscapeTextures/";
    inline constexpr wchar_t ResourceRoot_Dir[] = L"../../../Client/Bin/Resources/";
    inline constexpr wchar_t MF_Zone_Boss_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/MF_Zone_Boss.hmap";
    inline constexpr wchar_t MF_Zone_Main_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/Test_LS_MF_Zone.hmap";
    inline constexpr wchar_t MF_Zone_Stair_1_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/MF_Zone_Stair_1.hmap";
    inline constexpr wchar_t MF_Zone_Stair_2_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/MF_Zone_Stair_2.hmap";
    inline constexpr wchar_t MF_Zone_Stair_3_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/MF_Zone_Stair_3.hmap";
    inline constexpr wchar_t MF_Zone_Left_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/MF_Zone_Boss_Left.hmap";
    inline constexpr wchar_t MF_Zone_Right_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/MF_Zone_Boss_Right.hmap";

    inline constexpr wchar_t WaterPath[] = L"../../../Client/Bin/Resources/Map/Water/";

    // gestral
    inline constexpr wchar_t GestralVillage_MapInstance_Path[] = L"../../../Client/Bin/Resources/Map/MapInstance/Level_Main_GestralVillage_V2";
    inline constexpr wchar_t GestralVillage_HMap[] = L"../../../Client/Bin/Resources/Map/SaveJson/Gestral_Main.hmap";
}

namespace Client::Constants::Charcter
{
    inline constexpr int32 scaleOffset = 10000;
}

namespace Client::Constants::Prefab
{
    inline constexpr wchar_t Root_Dir[] = L"Data/json/Prefabs";
    inline constexpr wchar_t NPC_Dir[] = L"Data/json/Prefabs/NPC";
    inline constexpr wchar_t Player_Dir[] = L"Data/json/Prefabs/Player";
    inline constexpr wchar_t Monster_Dir[] = L"Data/json/Prefabs/Monster";
    inline constexpr wchar_t Item_Dir[] = L"Data/json/Prefabs/Item";
    inline constexpr wchar_t Weapon_Dir[] = L"Data/json/Prefabs/Weapons";

    inline constexpr wchar_t BattleStartSpawner_Dir[] = L"Data/json/Prefabs/BattleStartSpawner";

    inline constexpr wchar_t VictorialFlag_Dir[] = L"Data/json/Prefabs/ETC/VictoriaFlag";

    inline constexpr char Item_Pictos_WarmingUp[] = "Item_Pictos_WarmingUp";

#pragma region 플레이어
    // 월드
    inline constexpr char PW_Gustave[]  = "PW_Gustave";
    inline constexpr char PW_Lune[]     = "PW_Lune";
    inline constexpr char PW_Maelle[]   = "PW_Maelle";

    // 배틀
    inline constexpr char PB_Gustave[]  = "PB_Gustave";
    inline constexpr char PB_Lune[]     = "PB_Lune";
    inline constexpr char PB_Maelle[]   = "PB_Maelle";

    // 프리뷰
    inline constexpr char PP_Gustave[]  = "PP_Gustave";
    inline constexpr char PP_Lune[]     = "PP_Lune";
    inline constexpr char PP_Maelle[]   = "PP_Maelle";
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

    inline constexpr char MW_Mime[]             = "MW_Mime";
    inline constexpr char MB_Mime[]             = "MB_Mime";

    inline constexpr char MW_PotatoBag[]        = "MW_PotatoBag";
    inline constexpr char MB_PotatoBag[]        = "MB_PotatoBag";

    inline constexpr char MW_Simon[]            = "MW_Simon";
    inline constexpr char MB_Simon[]            = "MB_Simon";
    inline constexpr char MB_Simon_V2[]         = "MB_Simon_V2";

    // 테스트용
    inline constexpr char SL_Test_Lancelier[]   = "SL_Test_Lancelier";

    // 쌍검
    inline constexpr char SL_Dualliste[]        = "SL_Dualliste";

#pragma endregion

#pragma region 무기
    // 구
    inline constexpr char Gustave_Lanceram[]        = "WP_Gustave_Lanceram";
    inline constexpr char Gustave_Lanceram_Gun[]    = "WP_Gustave_Lanceram_Gun";
    inline constexpr char Gustave_Baguette[]        = "WP_Gustave_Baguette";
    inline constexpr char Gustave_Baguette_Gun[]    = "WP_Gustave_Baguette_Gun";
    inline constexpr char Gustave_Tireso[]          = "WP_Gustave_Tireso";
    inline constexpr char Gustave_Tireso_Gun[]      = "WP_Gustave_Tireso_Gun";

    // 루
    inline constexpr char Lune_Colim[]              = "WP_Lune_Colim";
    inline constexpr char Lune_Baguette[]           = "WP_Lune_Baguette";
    inline constexpr char Lune_Snowim[]             = "WP_Lune_Snowim";

    // 마
    inline constexpr char Maelle_Medalum[]          = "WP_Maelle_Medalum";
    inline constexpr char Maelle_Baguette[]         = "WP_Maelle_Baguette";
    inline constexpr char Maelle_Plenum[]           = "WP_Maelle_Plenum";

    // 시몬
    inline constexpr char Simon_Weapon[]             = "WP_Simon";

    inline constexpr char Dualliste_Sword_L[]        = "WP_Dualliste_Sword_L";
    inline constexpr char Dualliste_Sword_R[]        = "WP_Dualliste_Sword_R";

    inline constexpr char PotatoBag_Weapon[] = "WP_PotatoBag";

#pragma endregion

    inline constexpr char Eye_Target[]      = "ETC/EyeTarget";
    inline constexpr char SoulLikeController[] = "ETC/SoulLikeController";
    inline constexpr char Portal_BishopReward[] = "ETC/Portal_BishopReward";
    inline constexpr char Portal_GestralReward[] = "ETC/Portal_GestralReward";
    inline constexpr char Trigger_Cinematic[] = "ETC/Trigger_Cinematic";
    inline constexpr char Trigger_BGMZone[] = "ETC/Trigger_BGMZone";
    inline constexpr char Trigger_WorldDialogue[] = "ETC/Trigger_WorldDialogue";

    inline constexpr char BattleStartSpawner_Boss[] = "BattleStartSpawner/BSS_GamePlay_Boss";
    inline constexpr char BSS_Gestral_PotatoBag[] = "BattleStartSpawner/BSS_Gestral_PotatoBag";

    inline constexpr char BSS_Gameplay01[] = "BattleStartSpawner/BSS_GamePlay01";
    inline constexpr char BSS_Gameplay02[] = "BattleStartSpawner/BSS_GamePlay02";


}

// 이제 사운드는 DT_SoundCue에 추가해서, 거기에 등록된 cueId를 여기에 상수로 매핑하는 형식으로 통일하기
namespace Client::Constants::SoundBGM
{
    constexpr const wchar_t* MainTitleBGM = L"MainTitle/MainTitle_BGM.wav";
    constexpr const wchar_t* BattleNormalBGM = L"BGM/Battle/MUS_SpringMeadows_BT_BattleTheme_Intro [1].wav";
    constexpr const wchar_t* BattleBossBGM = L"BGM/Battle/MUS_SpringMeadows_BT_BattleTheme_Main [1].wav";
    constexpr const wchar_t* SpringBGM = L"BGM/SpringMeadow/MUS_SpringMeadows_ENV_Nightfall_PartA.wav";

    constexpr const wchar_t* GestralVillageMarket =
        L"BGM/Gestral/MUS_GestralVillage_ENV_Market_PartA.wav";
    constexpr const wchar_t* GestralVillageGolgraThrone =
        L"BGM/Gestral/MUS_GestralVillage_ENV_GolgraThrone.wav";

    constexpr const wchar_t* PotatoBagBattleMain =
        L"BGM/Gestral/MUS_AncientSanctuary_BT_Summer2022_Main.wav";
    constexpr const wchar_t* PotatoBagBattleTransition =
        L"BGM/Gestral/MUS_AncientSanctuary_BT_Summer2022_Transition.wav";

    constexpr const wchar_t* DuallisteBattlePhase1Main =
        L"BGM/Gestral/MUS_ForgottenBattlefield_BT_Dualliste_Phase1_Main.wav";
    constexpr const wchar_t* DuallisteBattlePhaseTransition =
        L"BGM/Gestral/MUS_ForgottenBattlefield_BT_Dualliste_PhaseTransition.wav";
    constexpr const wchar_t* DuallisteBattlePhase2Main =
        L"BGM/Gestral/MUS_ForgottenBattlefield_BT_Dualliste_Phase2_Main.wav";

    constexpr const wchar_t* BishopBattleBossMain = L"BGM/SpringMeadow/MUS_SpringMeadows_BT_Boss_Main.wav";
    constexpr const wchar_t* BishopBattleBossVictory = L"BGM/SpringMeadow/MUS_SpringMeadows_BT_Boss_VictoryScreen.wav";

    constexpr const wchar_t* LancelierIntroCinematic =
        L"BGM/SpringMeadow/MUS_SpringMeadows_CIN_FirstLancelier_Intro [1].wav";
    constexpr const wchar_t* LancelierBattleMain =
        L"BGM/SpringMeadow/MUS_SpringMeadows_BT_FirstLancelier_Main [1].wav";

    constexpr const wchar_t* SimonBattlePhase1Intro = L"BGM/Simon/MUS_Common_BT_Simon_DivergentStars_Main.wav";
    constexpr const wchar_t* SimonBattlePhase1Main = L"BGM/Simon/MUS_Simon_BT_WeLost_Phase1_Main.wav";

    constexpr const wchar_t* SimonBattlePhase1Transition = L"BGM/Simon/MUS_Common_BT_Simon_DivergentStars_Transition.wav";

    constexpr const wchar_t* SimonBattlePhase2Intro = L"BGM/Simon/MUS_Simon_BT_WeLost_Phase2_Intro.wav";
    constexpr const wchar_t* SimonBattlePhase2Main = L"BGM/Simon/MUS_Simon_BT_WeLost_Phase2_Main.wav";

    constexpr const wchar_t* SimonStartWindBGM = L"BGM/Simon/AMB_PAD_SeaCliff_Main [1].wav";

    constexpr const wchar_t* SimonIntroCinematic =
        L"BGM/Simon/MUS_Simon_CIN_SimonIntro.wav";

    constexpr const wchar_t* SimonOutroCinematic =
        L"BGM/Simon/MUS_Simon_CIN_SimonOutro.wav";

}

namespace Client::Constants::Cinematic
{
    inline constexpr wchar_t RuntimeJson_Dir[] = L"../../../Client/Bin/Resources/Data/json/Cinematics/";
    inline constexpr wchar_t BridgeCinematic[] = L"../../../Client/Bin/Resources/Data/json/Cinematics/Bridge.json";
    inline constexpr wchar_t BishopCinematicFileId[] = L"Bishop_Cinematic_2.json";
    inline constexpr wchar_t TutorialCinematicFileId[] = L"Lancelier_Intro.json";

    inline constexpr wchar_t Dualliste_Intro[] = L"Dualliste_Intro.json";
    inline constexpr wchar_t Dualliste_PhaseChange[] = L"Dualliste.json";

    inline constexpr wchar_t SimonIntroFileId[] = L"Simon_Intro.json";
    inline constexpr wchar_t SimonOutro3FileId[] = L"Simon_Outro.json";
}

namespace Client::Constants::SoundCue
{
    constexpr const wchar_t* UI_MainTitle_ButtonHover = L"UI_MainTitle_ButtonHover";
    constexpr const wchar_t* UI_MainTitle_Loading = L"UI_MainTitle_Loading";
    constexpr const wchar_t* UI_MainTitle_NewGame = L"UI_MainTitle_NewGame";
    constexpr const wchar_t* UI_MainTitle_QuitGame = L"UI_MainTitle_QuitGame";

    constexpr const wchar_t* UI_SavePoint_Open = L"UI_SavePoint_Open";
    constexpr const wchar_t* UI_SavePoint_Close = L"UI_SavePoint_Close";
    constexpr const wchar_t* UI_SavePoint_Resting = L"UI_SavePoint_Resting";

    constexpr const wchar_t* UI_Dialogue_CanvasEnter = L"UI_Dialogue_CanvasEnter";
    constexpr const wchar_t* UI_Journal_Open = L"UI_Journal_Open";
    constexpr const wchar_t* UI_Journal_Close = L"UI_Journal_Close";
    constexpr const wchar_t* UI_Global_GenericEnter = L"UI_Global_GenericEnter";
    constexpr const wchar_t* UI_Global_GenericExit = L"UI_Global_GenericExit";
    constexpr const wchar_t* UI_Global_NavigateClick = L"UI_Global_NavigateClick";
    constexpr const wchar_t* UI_Global_NavigateClickBack = L"UI_Global_NavigateClickBack";
    constexpr const wchar_t* UI_Global_NavigateClickValid = L"UI_Global_NavigateClickValid";
    constexpr const wchar_t* UI_Global_NavigateClickPaint = L"UI_Global_NavigateClickPaint";
    constexpr const wchar_t* UI_Global_NavigateClickPaintBack = L"UI_Global_NavigateClickPaintBack";
    constexpr const wchar_t* UI_Global_NavigateClickPaintValid = L"UI_Global_NavigateClickPaintValid";
    constexpr const wchar_t* UI_Global_NavigatePaint = L"UI_Global_NavigatePaint";
    constexpr const wchar_t* UI_Global_PaintEnter = L"UI_Global_PaintEnter";
    constexpr const wchar_t* UI_Global_PaintExit = L"UI_Global_PaintExit";
    constexpr const wchar_t* UI_Global_InterfaceClickMenu = L"UI_Global_InterfaceClickMenu";
    constexpr const wchar_t* UI_Level_Announce = L"UI_Level_Announce";

    constexpr const wchar_t* UI_Attribute_ChangeValue = L"UI_Attribute_ChangeValue";
    constexpr const wchar_t* UI_Attribute_Navigate = L"UI_Attribute_Navigate";
    constexpr const wchar_t* UI_Attribute_UpgradeAccept = L"UI_Attribute_UpgradeAccept";
    constexpr const wchar_t* UI_Attribute_UpgradeLoad = L"UI_Attribute_UpgradeLoad";
    constexpr const wchar_t* UI_Attribute_UpgradeLoadMagic = L"UI_Attribute_UpgradeLoadMagic";
    constexpr const wchar_t* UI_Attribute_UpgradeLoadMecanism = L"UI_Attribute_UpgradeLoadMecanism";
    constexpr const wchar_t* UI_Attribute_UpgradeLoadStop = L"UI_Attribute_UpgradeLoadStop";

    constexpr const wchar_t* UI_ScoreBoard_Appear = L"UI_ScoreBoard_Appear";
    constexpr const wchar_t* UI_ScoreBoard_Disappear = L"UI_ScoreBoard_Disappear";
    constexpr const wchar_t* UI_Tuto_Appear = L"UI_Tuto_Appear";

    constexpr const wchar_t* UI_Battle_SelectAlly = L"UI_Battle_SelectAlly";
    constexpr const wchar_t* UI_Battle_SelectEnemy = L"UI_Battle_SelectEnemy";
    constexpr const wchar_t* UI_Battle_Encounter = L"UI_Battle_Encounter";
    constexpr const wchar_t* UI_Battle_StateEnd = L"UI_Battle_StateEnd";
    constexpr const wchar_t* UI_Battle_NoAP = L"UI_Battle_NoAP";
    constexpr const wchar_t* UI_Battle_Start = L"UI_Battle_Start";
    constexpr const wchar_t* UI_Battle_TargetAllyOpen = L"UI_Battle_TargetAllyOpen";
    constexpr const wchar_t* UI_Battle_TargetAllyClose = L"UI_Battle_TargetAllyClose";
    constexpr const wchar_t* UI_Battle_TargetEnemyOpen = L"UI_Battle_TargetEnemyOpen";
    constexpr const wchar_t* UI_Battle_TargetEnemyClose = L"UI_Battle_TargetEnemyClose";
    constexpr const wchar_t* UI_Battle_TurnPass = L"UI_Battle_TurnPass";
    constexpr const wchar_t* UI_Battle_TurnStartAlly = L"UI_Battle_TurnStartAlly";
    constexpr const wchar_t* UI_Battle_TurnStartEnemy = L"UI_Battle_TurnStartEnemy";
    constexpr const wchar_t* UI_Skill_NavigatePage = L"UI_Skill_NavigatePage";
    constexpr const wchar_t* UI_Battle_FleeActivation = L"UI_Battle_FleeActivation";
    constexpr const wchar_t* UI_Battle_FleeSelect = L"UI_Battle_FleeSelect";
    constexpr const wchar_t* UI_Battle_ItemMenuOpen = L"UI_Battle_ItemMenuOpen";
    constexpr const wchar_t* UI_Battle_ItemMenuClose = L"UI_Battle_ItemMenuClose";
    constexpr const wchar_t* UI_Battle_ItemTargetAllyStart = L"UI_Battle_ItemTargetAllyStart";
    constexpr const wchar_t* UI_Battle_ItemTargetAllyStop = L"UI_Battle_ItemTargetAllyStop";
    constexpr const wchar_t* UI_Battle_QTE_ValidationPerfect = L"UI_Battle_QTE_ValidationPerfect";
    constexpr const wchar_t* UI_Battle_TargetEnemyStart = L"UI_Battle_TargetEnemyStart";
    constexpr const wchar_t* UI_Battle_TargetEnemyStop = L"UI_Battle_TargetEnemyStop";
    constexpr const wchar_t* UI_Battle_SpellsMenuOpen = L"UI_Battle_SpellsMenuOpen";
    constexpr const wchar_t* UI_Battle_SpellsMenuClose = L"UI_Battle_SpellsMenuClose";
    constexpr const wchar_t* UI_Battle_SpellsMoreOpen = L"UI_Battle_SpellsMoreOpen";

    constexpr const wchar_t* UI_Battle_QTE_Start = L"UI_Battle_QTE_Start";
    constexpr const wchar_t* UI_Battle_QTE_Miss = L"UI_Battle_QTE_Miss";
    constexpr const wchar_t* UI_Battle_QTE_SuccessPerfect = L"UI_Battle_QTE_SuccessPerfect";
    constexpr const wchar_t* UI_Battle_QTE_SuccessRegular = L"UI_Battle_QTE_SuccessRegular";
    constexpr const wchar_t* SFX_Dodge_Success = L"SFX_Dodge_Success";
    constexpr const wchar_t* SFX_UI_Dodge_Success = L"SFX_UI_Dodge_Success";
    constexpr const wchar_t* SFX_FreeAimStart = L"SFX_FreeAimStart";
    constexpr const wchar_t* SFX_FreeAimStop = L"SFX_FreeAimStop";
    constexpr const wchar_t* SFX_FreeAimHit = L"SFX_FreeAimHit";
    constexpr const wchar_t* UI_SkillTree_Navigate = L"UI_SkillTree_Navigate";
    constexpr const wchar_t* UI_SkillTree_Learn = L"UI_SkillTree_Learn";
    constexpr const wchar_t* UI_SkillTree_Blocked = L"UI_SkillTree_Blocked";
    constexpr const wchar_t* UI_SkillTree_Equip = L"UI_SkillTree_Equip";
    constexpr const wchar_t* UI_SkillTree_Unequip = L"UI_SkillTree_Unequip";

    constexpr const wchar_t* SFX_Eveque_Earth_SpikeHit = L"SFX_Eveque_Earth_SpikeHit";

    constexpr const wchar_t* Battle_Parry_Success = L"Battle_Parry_Success";

    constexpr const wchar_t* Duallist_Attack = L"VO_ENM_Duallist_Attack";
    constexpr const wchar_t* Duallist_Pain = L"VO_ENM_Duallist_Pain";

    constexpr const wchar_t* Gestral_Potato_Announce = L"Gestral_Potato_Announce";


}

namespace Client::Constants::Effect
{
    constexpr const wchar_t* BreakEffect = L"Effects\\Assets\\Etc\\Break.effect.json";

    constexpr const wchar_t* SL_Slash_L= L"Effects\\Assets\\Player\\Soullike_SwordSlash_L.effect.json";
    constexpr const wchar_t* SL_Slash_R = L"Effects\\Assets\\Player\\Soullike_SwordSlash_R.effect.json";

}

namespace Client::Constants::Dialogue
{
    constexpr const wchar_t* Dialogue_Dir = L"../../../Client/Bin/Resources/Data/json/Dialogue/Dialogues.json";
}

namespace Client::Constants::Value
{
    constexpr uint32 potatoBagEncounterId = 100; // 일반 월드 몬스터 기본 encounter 1과 분리된 Gradient 안내 전용 전투
    constexpr int32  potatoBagBattleStartSpawnerId = 5;
}
