#include "PreviewShotTable.h"

PreviewShotTable::PreviewShotTable()
{
}

PreviewShotTable::~PreviewShotTable()
{
}

PreviewShotTable::PreviewShotPreset PreviewShotTable::Get(HeroId heroId, PreviewShotPanel panel)
{
    PreviewShotPreset preset {};
     
    if(panel == PreviewShotPanel::CharacterStatus)
    {
        switch(heroId)
        {
        case HeroId::Gustave:
            {
                preset.lookOffset = Vec3( -0.55f, 1.6f, -3.4f);
                preset.followSpeed = 5.f;
                preset.distance = 5.f;
            }
            break;
            

        case HeroId::Lune:
            {
                preset.lookOffset = Vec3(-0.7f, 1.4f, -3.1f);
                preset.followSpeed = 5.f;
                preset.distance = 5.f;

            }
            break;

        case HeroId::Maelle:
            {
                preset.lookOffset = Vec3(-0.75f, 1.3f, -3.f);
                preset.followSpeed = 5.f;
                preset.distance = 5.f;
            }
            break;

        }
    }
         
    if(panel == PreviewShotPanel::SkillTree)
    {
        switch(heroId)
        {
        case HeroId::Gustave:
            {
                preset.lookOffset = Vec3(-1.65f,1.6f,-3.5f);
                preset.followSpeed = 3.f;
                preset.distance = 5.f;
            }
            break;
        case HeroId::Lune:
            {
                preset.lookOffset = Vec3(0.5f, 1.4f, -3.1f);
                preset.followSpeed = 3.f;
                preset.distance = 5.f;

            }
            break;
        case HeroId::Maelle:
            {
                preset.lookOffset = Vec3(-0.75f, 1.3f, -3.f);
                preset.followSpeed = 3.f;
                preset.distance = 5.f;
            }
            break;

        }
    }

    return preset;
}

Unique<PreviewShotTable> PreviewShotTable::Create()
{
    return make_unique<PreviewShotTable>();
}

void PreviewShotTable::Free()
{
    __super::Free();
}
