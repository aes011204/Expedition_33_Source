#include "UI_Dialogue.h"
#include "UI_RuntimeLoader.h"

HRESULT UI_Dialogue::Initialize(uint32 levelIndex)
{
    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    return S_OK;
}

void UI_Dialogue::Update(float timeDelta)
{
}

HRESULT UI_Dialogue::Load_UI(uint32 levelIndex)
{
    //UI_RuntimeLoader::UI_LoadDesc desc{};
    //desc.levelIndex = levelIndex;
    //desc.jsonPath = Constants::UI::UI_D;
    //desc.prototypeTagPrefix = L"Dialogue";
    //desc.outUIByName = &_uiByName;
    //desc.animationPath = Constants::UI_Anim::UI_GetCharacterAnim_Path;

    return S_OK;
}

HRESULT UI_Dialogue::Bind_UI()
{
    return S_OK;
}

Unique<UI_Dialogue> UI_Dialogue::Create(uint32 levelIndex)
{
    return make_unique<UI_Dialogue>();
}
