#pragma once

#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class Buff : public Base
{
public:
    Buff() = default;
    ~Buff() override = default;

public:
    void    Initialize(Buffs buffId, uint32 stack = 1, int32 durationTurn = 0)
    {
        _buffId = buffId;
        _stack = max<uint32>(stack, 1);
        _durationTurn = durationTurn;
        _pendingRemove = false;
    }

    Buffs   Get_BuffId() const { return _buffId; }
    uint32  Get_Stack() const { return _stack; }
    int32   Get_DurationTurn() const { return _durationTurn; }
    bool    Is_PendingRemove() const { return _pendingRemove; }

    void    Mark_Remove() { _pendingRemove = true; }; // 버프 시스템에서 처리
    void    Add_Stack(uint32 amount) { _stack += amount; }

    void    Reduce_DurationTurn() // 지속 턴
    {
        if (_durationTurn > 0)
            --_durationTurn;

        if (_durationTurn == 0)
            _pendingRemove = true;
    }

    void    Refresh_DurationTurn(int32 durationTurn)
    {
        _durationTurn = durationTurn;   // 같은 상태이상을 다시 맞으면 새 지속 턴
        _pendingRemove = false;         // 갱신된 버프가 이번 턴에서 정리되지 않게
    }


private:
    Buffs   _buffId = Buffs::None;
    uint32  _stack = 1;
    int32   _durationTurn = 0;  // 일단 마크밖에 없으니까 무한유지
    bool    _pendingRemove = false;

public:
    static Shared<Buff> Create() { return make_shared<Buff>(); }

};

NS_END
