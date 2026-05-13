#pragma once

#include "Editor_Window.h"
#include "EffectMaterialAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class EffectMaterial_View final : public Editor_Window
{
public:
    EffectMaterial_View();
    ~EffectMaterial_View() override = default;

public:
    void Render() override;

public: //## Behavior::Selection
    void Open_Definition(const string& definitionName);

private: //## Data
    string _definitionName{ "SpriteSymbolTranslucent" };

private: //## Helpers
    const EffectMaterialDefinitionData& Get_CurrentDefinition() const;
    void Render_DefinitionInfo(const EffectMaterialDefinitionData& definition);

public:
    static Shared<EffectMaterial_View> Create();
};

NS_END
