#include "WorldLedgeSpot.h"

#include "Player_WorldBase.h"

IMPLEMENT_REFLECTION(WorldLedgeSpot)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "WorldLedge";
    info.category = "Trigger";

    PROPERTY_INT("Priority", _priority);
    PROPERTY_VEC3("Collider Center", _colliderCenter);
    PROPERTY_VEC3("Collider Extents", _colliderExtents);
    PROPERTY_VEC3("Start Point Offset", _startPointOffset);
    PROPERTY_VEC3("Target Point Offset", _targetPointOffset);
    PROPERTY_VEC3("Wall Point Offset", _wallPointOffset);
    PROPERTY_VEC3("Face Direction", _faceDirection);
    PROPERTY_FLOAT("Warp End Progress", _warpEndProgress, 0.01f, 1.f, 0.01f);
    PROPERTY_WSTRING("Prompt Text", _promptText);

    return true;
}

WorldLedgeSpot::WorldLedgeSpot(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : WorldInteractionSpot(device, context)
{
    _promptText = L"Ledge";
}

WorldLedgeSpot::WorldLedgeSpot(const WorldLedgeSpot& prototype)
    : WorldInteractionSpot(prototype)
{
}

bool WorldLedgeSpot::Build_InteractionRequest(const Shared<Player_WorldBase>& activeHero, WorldInteractionRequest& outRequest) const
{
    if (!Can_Interact(activeHero))
        return false;

    outRequest = {};
    outRequest.type = WorldInteractionType::WallClimb;
    Build_CommonRequest(outRequest);

    return true;
}

Shared<WorldLedgeSpot> WorldLedgeSpot::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<WorldLedgeSpot>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : WorldLedge");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> WorldLedgeSpot::Clone(void* arg)
{
    auto clone = make_shared<WorldLedgeSpot>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : WorldLedge");
        return nullptr;
    }

    return clone;
}

void WorldLedgeSpot::Free()
{
    WorldInteractionSpot::Free();
}
