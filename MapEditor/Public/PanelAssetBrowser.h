#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

class PanelAssetBrowser final : public MEWindow
{
public:
    PanelAssetBrowser();
    virtual ~PanelAssetBrowser() = default;

public:
    virtual void Render() override;

public:
    static Shared<PanelAssetBrowser> Create();
};

NS_END
