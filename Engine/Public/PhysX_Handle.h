#pragma once

#include <cstdint>

namespace Engine
{
    struct PhysXStaticActorHandle
    {
        std::uint64_t id = 0;

        bool Is_Valid() const
        {
            return id != 0;
        }
    };
}
