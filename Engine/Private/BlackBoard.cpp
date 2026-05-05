#include "BlackBoard.h"

BlackBoard::BlackBoard()
{
}

BlackBoard::~BlackBoard()
{
}

Shared<BlackBoard> BlackBoard::Create()
{
    auto instance = make_shared<BlackBoard>();

    //if (FAILED(instance->Initialize_Prototype()))
    //{
    //    MSG_BOX("Failed to Created : Monster");
    //    return nullptr;
    //}

    return instance;
}

void BlackBoard::Free()
{
    __super::Free();
}


