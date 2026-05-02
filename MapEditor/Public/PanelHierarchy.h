#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

class PanelHierarchy final : public MEWindow
{
public:
    PanelHierarchy();
    virtual ~PanelHierarchy() = default;

public:
    virtual void Render() override;

public:
    static Shared<PanelHierarchy> Create();
};

NS_END
