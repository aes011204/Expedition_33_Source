#pragma once
#include "Base.h"

class PreviewShotTable : public Base
{
public:
    enum class PreviewShotPanel
    {
        CharacterStatus,
        SkillTree
    };

    struct PreviewShotPreset
    {
        Vec3 lookOffset;
        float followSpeed;
        float distance;
    };

public:
    PreviewShotTable();
    virtual ~PreviewShotTable();

public:
    PreviewShotPreset Get(HeroId heroId, PreviewShotPanel panel);
    
public:
    static Unique<PreviewShotTable> Create();
    void Free() override;

};

