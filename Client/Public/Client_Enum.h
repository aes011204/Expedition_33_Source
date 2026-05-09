#pragma once

namespace Client
{
    enum class PlayerState
    {
        Idle, 
        Move,
        Jump,       // Drop도 간단하게 같이 처리 해야하는가?
        Attack, 

        Climb,
        RopeAction,
        End
    };

}

