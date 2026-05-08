#pragma once

#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

class Emitter_View final : public Editor_Window
{
public:
    Emitter_View();
    ~Emitter_View() override = default;

public:
    void Render() override;

public:
    static Shared<Emitter_View> Create();

private:
    struct Module_Row
    {
        string name{};
        string summary{};
        bool required{false};
        bool enabled{true};
    };

    struct Emitter_Row
    {
        string name{};
        string sourceSystem{};
        string rendererType{};
        string supportState{};
        string resourceSummary{};
        string materialName{};
        string textureId{};
        ImVec4 accentColor{};
        vector<Module_Row> modules{};
    };

private:
    void Draw_Toolbar();
    void Draw_EmitterBoard();
    void Draw_EmitterColumn(size_t emitterIndex, const Emitter_Row& emitter);
    void Draw_ModuleRow(const Module_Row& module);
    void Draw_SelectionSummary() const;

private:
    vector<Emitter_Row> _emitters{};
    size_t _selectedEmitterIndex{0};
};

NS_END
