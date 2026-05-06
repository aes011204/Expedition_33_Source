#include "BT_VIew.h"
#include "BT_Inspector.h"

BT_Inspector::BT_Inspector() : Editor_Window(L"BT_Inspector")
{
}

BT_Inspector::~BT_Inspector()
{
}

Shared<BT_Inspector> BT_Inspector::Create()
{
    return make_shared<BT_Inspector>();
}

void BT_Inspector::Free()
{
}
