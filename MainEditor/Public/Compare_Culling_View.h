#pragma once
#include "Editor_Window.h"

NS_BEGIN(MainEditor)

class Compare_Culling_View final : public Editor_Window
{
public:
    Compare_Culling_View();
    virtual ~Compare_Culling_View() override = default;

public:
    virtual void Update(float timeDelta) override;
    virtual void Render() override;

public:
    static Shared<Compare_Culling_View> Create();

private:
    void Draw_StaticMeshCullingBoxes();

private:
    bool _showStaticMeshCullBoxes = false;
};

NS_END
