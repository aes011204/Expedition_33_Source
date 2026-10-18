#pragma once

#include "Editor_Window.h"
#include "Renderer_Define.h"

NS_BEGIN(MainEditor)

class RenderCall_View final : public Editor_Window
{
public:
    RenderCall_View();
    virtual ~RenderCall_View() override = default;

public:
    virtual void Render() override;

private:
    struct Aggregated_Row
    {
        uintptr_t objectKey = 0;
        string name = {};
        string className = {};
        string sourceModel = {};
        uint32 count = 0;
        uint32 submissionCount = 0;
        uint32 instanceCount = 0;
        bool isInstanced = false;
    };

private:
    void Draw_GroupSection(
        const char* header,
        const Engine::RenderViewHotspotSnapshot& snapshot,
        Engine::RenderGroup renderGroup);
    vector<Aggregated_Row> Build_GroupRows(
        const Engine::RenderViewHotspotSnapshot& snapshot,
        Engine::RenderGroup renderGroup) const;

public:
    static Shared<RenderCall_View> Create();
};

NS_END
