#pragma once

namespace Client
{
    enum class PlayerState
    {
        Idle, 
        Walk,       // 연출용 인데 Move로 묶을까
        Move,
        Jump,       // Drop도 간단하게 같이 처리 해야하는가?
        Attack, 

        Climb,
        RopeAction,

    };

}

