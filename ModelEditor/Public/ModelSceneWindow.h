#pragma once

#include "EditorBlock.h"
#include <imgui.h>
#include <ImGuizmo.h>

NS_BEGIN(ModelEditor)

class ModelSceneWindow final : public EditorBlock
{
public:
    ModelSceneWindow() = default;
    ~ModelSceneWindow() override = default;

public:
    HRESULT Initialize() override;
    void Update(float timeDelta) override;
    void Render() override;

private:
    void Render_ViewportCanvas();

private:
    ImVec2 _viewportSize = {};

public:
    static Shared<ModelSceneWindow> Create();
};

NS_END
