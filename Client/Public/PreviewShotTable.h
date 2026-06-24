#pragma once

#include "Base.h"
#include "Client_Enum.h"
#include "CameraPresetTypes.h"

NS_BEGIN(Client)
class PreviewShotTable : public Base
{

public:
    PreviewShotTable();
    virtual ~PreviewShotTable();

public:
    PreviewShotPreset Get(HeroId heroId, PreviewShotPanel panel);
    
public:
    static Unique<PreviewShotTable> Create();
    void Free() override;

};

NS_END
