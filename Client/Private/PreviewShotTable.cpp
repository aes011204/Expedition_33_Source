#include "PreviewShotTable.h"
#include "CameraPresetIO.h"

PreviewShotTable::PreviewShotTable()
{
}

PreviewShotTable::~PreviewShotTable()
{
}

PreviewShotPreset PreviewShotTable::Get(HeroId heroId, PreviewShotPanel panel)
{
    PreviewShotPreset preset {};

    if(CameraPresetIO::LoadPreviewFromFile(panel, heroId ,preset))
    {
        return preset;
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
