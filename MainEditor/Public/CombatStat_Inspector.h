#pragma once
#include "Component_Inspector.h"

NS_BEGIN(Client)
class CombatStatCom;
NS_END

NS_BEGIN(MainEditor)

class CombatStat_Inspector : public Component_Inspector
{
public:
    CombatStat_Inspector() = default;
    ~CombatStat_Inspector() override = default;

protected:
    
    void Draw_CommonRuntimeSection(const Shared<Client::CombatStatCom>& stat);
};

NS_END
