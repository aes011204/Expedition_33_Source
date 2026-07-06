#pragma once
#include "Base.h"
#include "ChainModifier.h"
#include "PhysicsModifier.h"
#include "PoseFollowChainModifier.h"
#include "Client_Enum.h"

NS_BEGIN(Client)

class ChainModifierPreset final : public Base
{
public:
    static void BuildScarfDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"ClothPhysics_Scarf");

    static void BuildPoseFollowScarfDesc(
        PoseFollowChainModifier::PoseFollowChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"PoseFollow_Scarf");

    static void BuildArmbandDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"ClothPhysics_Armband");

    static void BuildBindPoseHairDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"BindPose_Hair");

    static void BuildAbbestClothDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"ClothPhysics_Abbest");

    static void BuildBishopClothDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"ClothPhysics_Eveque");

    static void BuildBishopLegClothDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"ClothPhysics_Eveque_Leg");

    static void BuildPotatoBagHairDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"ClothPhysics_PotatoBagHair");

    

    

    static void BuildSimonHairPhysicsDesc(
        PhysicsModifier::PhysicsModifierDesc& outDesc,
        TransformCom* ownerTransform,
        const wstring& modifierTag = L"PhysicsModifier_SimonHair");

    static void BuildDuallisteHairbandPhysicsDesc(
        PhysicsModifier::PhysicsModifierDesc& outDesc,
        TransformCom* ownerTransform,
        const wstring& modifierTag = L"PhysicsModifier_DuallisteHairband");

    static void BuildPotatoBagClothBackDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"ClothPhysics_PotatoBagCloth_Back");

    static void BuildGustavePotatoCapePhysicsDesc(
        PhysicsModifier::PhysicsModifierDesc& outDesc,
        TransformCom* ownerTransform,
        const wstring& modifierTag = L"PhysicsModifier_PotatoCape");

    static void BuildGustaveHairPhysicsDesc(
        PhysicsModifier::PhysicsModifierDesc& outDesc,
        TransformCom* ownerTransform,
        const wstring& modifierTag = L"PhysicsModifier_GustaveHair");

    static void BuildLuneHairPhysicsDesc(
        PhysicsModifier::PhysicsModifierDesc& outDesc,
        TransformCom* ownerTransform,
        const wstring& modifierTag = L"PhysicsModifier_LuneHair");

    static void BuildMaelleBackHairPhysicsDesc(
        PhysicsModifier::PhysicsModifierDesc& outDesc,
        TransformCom* ownerTransform,
        const wstring& modifierTag = L"PhysicsModifier_MaelleBackHair");

    static void BuildMaelleFrontHairPhysicsDesc(
        PhysicsModifier::PhysicsModifierDesc& outDesc,
        TransformCom* ownerTransform,
        const wstring& modifierTag = L"PhysicsModifier_MaelleFrontHair");

    static void BuildBackpackBandDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"ClothPhysics_BackpackBand");

    static void BuildCoatDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"ClothPhysics_Coat");

    static void BuildPoseFollowCoatDesc(
        PoseFollowChainModifier::PoseFollowChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"PoseFollow_Coat");

};

NS_END
