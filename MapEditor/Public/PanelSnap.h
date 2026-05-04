#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

class PanelSnap final : public MEWindow
{
public:
    PanelSnap(Shared<SnapSettings> settings);
    virtual ~PanelSnap() = default;

public:
    Shared<SnapSettings> GetSettings() const { return _settings; }

public:
    virtual void Update(float timeDelta) override {}
    virtual void Render() override;

private:
    Shared<SnapSettings> _settings;

public:
    static Shared<PanelSnap> Create(Shared<SnapSettings> settings);
};

NS_END
