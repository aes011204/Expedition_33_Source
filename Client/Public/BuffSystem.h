#pragma once
#include "Base.h"
#include "Battle_Enum.h"


namespace Client
{
    class CombatStatCom;

    class BuffSystem :
        public Base
    {


    public:
        void ProcessTurnStart(Shared<CombatStatCom>  statCom);
        void ProcessTurnEnd(Shared<CombatStatCom> statCom);

        void ApplyAilmentOnUpadate(Shared<CombatStatCom> statCom);

        void  OnModifyDamage(DamageContext  statCom);


        void AddBuff(BattleChar* target, BuffType type, EStainChangeReason reason);
        void RemoveBuff(BattleChar* target, BuffType type, EStainChangeReason reason);
        //void RemoveBuffInstance(BattleChar* target, BuffInstanceId id);
        void ClearBuffs(BattleChar* target/*, BuffClearRule rule*/, EStainChangeReason reason);

    };

}
