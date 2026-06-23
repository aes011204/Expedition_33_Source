#pragma once

#include "Engine_Macro.h"

NS_BEGIN(Engine)

enum class Collision_Channel : uint32
{
    NONE = 0,
    Player = 1 << 0,
    Player_Body = 1 << 1,
    Player_WorldBody = 1 << 2,
    Player_Attack = 1 << 3,
    Monster = 1 << 4,
    Monster_Body = 1 << 5,
    Monster_WorldBody = 1 << 6,
    Monster_Attack = 1 << 7,
    Weapon = 1 << 8,
    Projectile = 1 << 9,
    Item = 1 << 10,
    Trigger = 1 << 11,
    Enviroment = 1 << 12,
    Player_Target = 1 << 13,
    Camera = 1 << 14,
    WorldActionTrigger = 1 << 15,
    Monster_WeakPoint = 1 << 16,

    SL_PlayerBody = 1 << 17,
    SL_PlayerAttack = 1 << 18,
    SL_MonsterBody = 1 << 19,
    SL_MonsterAttack = 1 << 20,
    SL_TargetSensor = 1 << 21,

    CHANNEL_ALL = 0xFFFFFFFF,   // 모든 충돌 OK
    CHANNEL_NONE = 0x00000000,  // 모든 충돌 ㄴㄴ
};

// 충돌 프리셋
enum class Collision_Preset
{
    Custom,     // 직접 세팅

    Player, Player_Body, Player_WorldBody, Player_Attack,
    Monster, Monster_Body, Monster_WorldBody, Monster_Attack,

    Weapon,
    Projectile, // 투사체
    WorldActionTrigger, // 월드액션 전투 진입용 트리거

    Item,
    Trigger,
    Enviroment,

    Player_Target,
    Camera,
    Monster_WeakPoint,
    Player_WeakPointTarget,

    SL_PlayerBody,
    SL_PlayerAttack,
    SL_MonsterBody,
    SL_MonsterAttack,
    SL_TargetSensor,

    END
};

enum class CollisionResponse
{
    Ignore = 0,     // 충돌 무시
    Overlap = 1,    // 충돌 시 겹쳐짐, 이벤트만 발생
    Block = 2,      // 충돌 시 서로 밀어냄

    END
};

// blockMask에 있으면 Block, overlapMask에 있으면 overlap 둘다 아니면 Ignore 판정
inline CollisionResponse Get_Collision_Response(uint32 selfOverlapMask, uint32 selfBlockMask, Collision_Channel targetChannel)
{
    uint32 targetBit = ETOI(targetChannel);

    if (selfBlockMask & targetBit)
        return CollisionResponse::Block;

    if (selfOverlapMask & targetBit)
        return CollisionResponse::Overlap;

    return CollisionResponse::Ignore;
}

// 둘 중 하나라도 Ignore -> Ignore
// Ignore가 아니고 둘 중 하나라도 Overlap이면 Overlap,
// 둘 다 Block이면 최종적으로 Block 판정이 되도록
inline CollisionResponse Calculate_ResponseResult(
    Collision_Channel selfType, uint32 selfOverlap, uint32 selfBlock,
    Collision_Channel otherType, uint32 otherOverlap, uint32 otherBlock)
{
    CollisionResponse selfResponse = Get_Collision_Response(selfOverlap, selfBlock, otherType);
    CollisionResponse otherResponse = Get_Collision_Response(otherOverlap, otherBlock, selfType);

    if (selfResponse == CollisionResponse::Ignore || otherResponse == CollisionResponse::Ignore)
        return CollisionResponse::Ignore;

    if (selfResponse == CollisionResponse::Overlap || otherResponse == CollisionResponse::Overlap)
        return CollisionResponse::Overlap;

    return CollisionResponse::Block;
}

struct Collision_Preset_Data
{
    Collision_Channel channel; // 자기 채널

    uint32 overlapMask;
    uint32 blockMask;

};

// 프리셋 테이블
static const Collision_Preset_Data g_CollisionPresets[ETOI(
    Collision_Preset::END)] =
{
    // Custom — 빈 값, 직접 설정
    { Collision_Channel::CHANNEL_NONE, 0, 0 },

    // ── Player ──
    { Collision_Channel::Player,
    /* overlapMask */ ETOI(Collision_Channel::Monster_Attack) |
                      ETOI(Collision_Channel::Item) |
                      ETOI(Collision_Channel::Trigger) |
                      ETOI(Collision_Channel::Projectile),
    /* blockMask   */ ETOI(Collision_Channel::Enviroment)
    },

    { Collision_Channel::Player_Body,
    /* overlapMask */ ETOI(Collision_Channel::Monster_Attack) |
                      ETOI(Collision_Channel::Item) |
                      ETOI(Collision_Channel::Trigger) |
                      ETOI(Collision_Channel::Projectile) |
                      ETOI(Collision_Channel::Monster_Body),

    /* blockMask   */ ETOI(Collision_Channel::Enviroment)
    },

    { Collision_Channel::Player_WorldBody,
    /* overlapMask */ ETOI(Collision_Channel::Monster_Attack) |
                      ETOI(Collision_Channel::Item) |
                      ETOI(Collision_Channel::Trigger) |
                      ETOI(Collision_Channel::Monster_WorldBody),

    /* blockMask   */ ETOI(Collision_Channel::Enviroment)
    },

    { Collision_Channel::Player_Attack,
    /* overlapMask */ ETOI(Collision_Channel::Monster) |
                      ETOI(Collision_Channel::Monster_Body),
    /* blockMask   */ 0
    },

    { Collision_Channel::Monster,
    /* overlapMask */ ETOI(Collision_Channel::Player_Attack) |
                      ETOI(Collision_Channel::Weapon) |
                      ETOI(Collision_Channel::Projectile) |
                      ETOI(Collision_Channel::Player_Target),
    /* blockMask   */ 0
    },

    { Collision_Channel::Monster_Body,
    /* overlapMask */ ETOI(Collision_Channel::Player_Attack) |
                      ETOI(Collision_Channel::Weapon) |
                      ETOI(Collision_Channel::Projectile) |
                      ETOI(Collision_Channel::Player_Target) |
                      ETOI(Collision_Channel::Player_Body),
    /* blockMask   */ ETOI(Collision_Channel::Enviroment)
    },

    { Collision_Channel::Monster_WorldBody,
    /* overlapMask */ ETOI(Collision_Channel::Player_Body) |
                      ETOI(Collision_Channel::Player_WorldBody) |
                      ETOI(Collision_Channel::Projectile) |
                      ETOI(Collision_Channel::Player_Target) |
                      ETOI(Collision_Channel::WorldActionTrigger),
    /* blockMask   */ ETOI(Collision_Channel::Enviroment)
   },


       { Collision_Channel::Monster_Attack,
    /* overlapMask */ ETOI(Collision_Channel::Player) |
                      ETOI(Collision_Channel::Player_Body),
    /* blockMask   */ 0
    },

    { Collision_Channel::Weapon,
    /* overlapMask */ ETOI(Collision_Channel::Monster) |
                      ETOI(Collision_Channel::Monster_Body),
    /* blockMask   */ 0
    },

    { Collision_Channel::Projectile,
    /* overlapMask */ ETOI(Collision_Channel::Monster) |
                      ETOI(Collision_Channel::Player),
    /* blockMask   */ ETOI(Collision_Channel::Enviroment)
    },

    { Collision_Channel::WorldActionTrigger,
    /* overlapMask */ ETOI(Collision_Channel::Monster_WorldBody),
    /* blockMask   */ ETOI(Collision_Channel::Enviroment)
    },

    { Collision_Channel::Item,
    /* overlapMask */ ETOI(Collision_Channel::Player),
    /* blockMask   */ 0
    },

    { Collision_Channel::Trigger,
    /* overlapMask */ ETOI(Collision_Channel::Player) |
                      ETOI(Collision_Channel::Player_Body) |
                      ETOI(Collision_Channel::Player_WorldBody),
    /* blockMask   */ 0
    },

    // Enviroment — 환경물. 플레이어/투사체를 Block
    { Collision_Channel::Enviroment,
    /* overlapMask */ 0,
    /* blockMask   */ ETOI(Collision_Channel::Player) |
                      ETOI(Collision_Channel::Player_Body) |
                      ETOI(Collision_Channel::Player_WorldBody) |
                      ETOI(Collision_Channel::Projectile) |
                      ETOI(Collision_Channel::Monster_Body) |
                      ETOI(Collision_Channel::Monster_WorldBody) |
                      ETOI(Collision_Channel::SL_PlayerBody) |
                      ETOI(Collision_Channel::SL_MonsterBody) |
                      ETOI(Collision_Channel::Camera)
    },

    // Player_Target — 타겟팅용. 몬스터/몬스터 몸체에 Overlap
    { Collision_Channel::Player_Target,
    /* overlapMask */ ETOI(Collision_Channel::Monster) |
                      ETOI(Collision_Channel::Monster_Body) |
                      ETOI(Collision_Channel::Monster_WorldBody) |
                      ETOI(Collision_Channel::Monster_WeakPoint),
    /* blockMask   */ 0
    },

    // Camera — 카메라 충돌 환경물만 Block`
    { Collision_Channel::Camera,
    /* overlapMask */ 0,
    /* blockMask   */ ETOI(Collision_Channel::Enviroment)
    },

    // Monster_WeakPoint — 자유 조준 약점 전용. 일반 몸통과 분리해 우선 query할 수 있게 한다.
    { Collision_Channel::Monster_WeakPoint,
    /* overlapMask */ ETOI(Collision_Channel::Player_Target),
    /* blockMask   */ 0
    },

    // Player_WeakPointTarget — 약점만 먼저 찾는 자유 조준 query 전용.
    { Collision_Channel::Player_Target,
    /* overlapMask */ ETOI(Collision_Channel::Monster_WeakPoint),
    /* blockMask   */ 0
    },

    // SL_PlayerBody: SL 모드 플레이어 본체/피격용. 월드 전투 진입 preset과 분리한다.
    { Collision_Channel::SL_PlayerBody,
      ETOI(Collision_Channel::SL_MonsterAttack),
      ETOI(Collision_Channel::Enviroment)
    },

    // SL_PlayerAttack: Maelle 무기/스킬 공격 판정. SL 몬스터 body만 때린다.
    { Collision_Channel::SL_PlayerAttack,
      ETOI(Collision_Channel::SL_MonsterBody),
      0
    },

    // SL_MonsterBody: SL 몬스터 본체/피격/락온 후보. PlayerAttack과 TargetSensor만 받는다.
    { Collision_Channel::SL_MonsterBody,
      ETOI(Collision_Channel::SL_PlayerAttack) |
      ETOI(Collision_Channel::SL_TargetSensor),
      ETOI(Collision_Channel::Enviroment)
    },

    // SL_MonsterAttack: SL 몬스터 공격 판정. SL 플레이어 body만 때린다.
    { Collision_Channel::SL_MonsterAttack,
      ETOI(Collision_Channel::SL_PlayerBody),
      0
    },

    // SL_TargetSensor: SL_TargetCom의 후보 탐색 sphere. SL 몬스터 body만 찾는다.
    { Collision_Channel::SL_TargetSensor,
      ETOI(Collision_Channel::SL_MonsterBody),
      0
    },
};

inline const Collision_Preset_Data& Get_PresetData(Collision_Preset preset)
{
    uint32 index = ETOI(preset);

    if (index >= ETOI(Collision_Preset::END))
        index = 0;

    return g_CollisionPresets[index];
}

NS_END
