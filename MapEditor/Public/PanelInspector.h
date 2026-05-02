#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

class PanelInspector final : public MEWindow
{
public:
    PanelInspector();
    virtual ~PanelInspector() = default;

public:
    virtual void Render() override;

public:
    static Shared<PanelInspector> Create();
};

NS_END
