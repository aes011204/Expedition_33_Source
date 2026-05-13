#pragma once

NS_BEGIN(EffectEditor)

struct EffectMaterialInstanceData
{
    // Definition에서 만든 embedded 사본이 실제 편집/저장할 값이다.
    string definitionName{ "SpriteSymbolTranslucent" };

    string mainTextureId{ "Texture_Effect_DefaultTexture" };
    string noiseTextureId{};

    Color tint{ 1.f, 1.f, 1.f, 1.f };

    float intensity{ 1.f };
    float opacityPower{ 1.f };
    float noiseStrength{ 0.f };

    string blendMode{ "AlphaBlend" };
    string opacitySource{ "Alpha" };
    uint32 subUvRows{ 1 };
    uint32 subUvCols{ 1 };
};

struct EffectMaterialDefinitionData
{
    // v1에서는 registry나 live asset 참조가 아니라 schema/default 틀만 표현한다.
    string name{ "SpriteSymbolTranslucent" };
    string rendererFamily{ "Sprite" };
    EffectMaterialInstanceData defaultInstance{};
};

inline const vector<EffectMaterialDefinitionData>& Get_EffectMaterialDefinitions()
{
    static const vector<EffectMaterialDefinitionData> definitions{
        EffectMaterialDefinitionData{
            "SpriteSymbolTranslucent",
            "Sprite",
            EffectMaterialInstanceData{}
        }
    };

    return definitions;
}

inline const EffectMaterialDefinitionData& Resolve_EffectMaterialDefinition(const string& definitionName)
{
    const vector<EffectMaterialDefinitionData>& definitions = Get_EffectMaterialDefinitions();
    for (const EffectMaterialDefinitionData& definition : definitions)
    {
        if (definition.name == definitionName)
            return definition;
    }

    return definitions.front();
}

NS_END
