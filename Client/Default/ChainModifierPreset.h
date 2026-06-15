#pragma once
#include "Base.h"
#include "ChainModifier.h"
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

    static void BuildArmbandDesc(
        ChainModifier::ChainModifierDesc& outDesc,
        TransformCom* ownerTransform,
        PlayerCharacterType characterType,
        const wstring& modifierTag = L"ClothPhysics_Armband");

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


};

NS_END
