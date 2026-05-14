#include "BuffSystem.h"

#include "Buff.h"
#include "CombatStatCom.h"

void BuffSystem::ProcessTurnStart(Shared<CombatStatCom> statCom)
{
    vector<Shared<Buff>> _buff = statCom->Get_Buff();

    for (int i = 0; i < _buff.size(); i++)
    {
        _buff[i]->OnTurnStart();
    }
}

void BuffSystem::ProcessTurnEnd(Shared<CombatStatCom> statCom)
{
    auto& _buff = statCom->Get_Buff();
    
    for (auto it = _buff.begin();
        it != _buff.end();)
    {
        (*it)->OnTurnEnd();

        if ((*it)->Is_MarkDestroy()== true)
        {
            it = _buff.erase(it);
        }
        else
        {
            ++it;
        }
    }
 


}

void BuffSystem::OnModifyDamage(DamageContext  _damageContext)
{
    vector<Shared<Buff>> _buff = _damageContext.target.statCom->Get_Buff();

    for (int i = 0; i < _buff.size(); i++)
    {
        _buff[i]->OnModifyDamage(_damageContext);
    }

}


