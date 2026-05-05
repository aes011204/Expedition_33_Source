#include "BT_Tool.h"

BT_Tool::BT_Tool(): Editor_Window(L"BT_TOOL")
{
}

HRESULT BT_Tool::Initialize()
{
    return S_OK;
}

void BT_Tool::Update(float timeDelta)
{
   
}

void BT_Tool::Render()
{
    
}

Shared<BT_Tool> BT_Tool::Create()
{
    return make_shared<BT_Tool>();
}

void BT_Tool::Free()
{
    __super::Free();
}
