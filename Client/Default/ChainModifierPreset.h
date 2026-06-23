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

    static void BuildPoseFollowHairDesc(
        PoseFollowChainModifier::PoseFollowChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"PoseFollow_Hair");

    static void BuildBindPoseHairDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"BindPose_Hair");

    static void BuildMaelleHairChainDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        const wstring& modifierTag = L"ClothPhysics_MaelleHair");

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

    static void BuildSimonPhysicsHairDesc(
        PhysicsModifier::PhysicsModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"Physics_SimonHair");

    static void BuildPotatoBagClothBackDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"ClothPhysics_PotatoBagCloth_Back");

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
