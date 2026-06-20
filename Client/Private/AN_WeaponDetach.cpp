#include "AN_WeaponDetach.h"

#include "ContainerObject.h"
#include "Mon_Simon_Weapon.h"
#include "TransformCom.h"

IMPLEMENT_REFLECTION(AN_WeaponDetach)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Part Tag", _partTag);
    PROPERTY_BOOL("Apply Scale", _applyScale);
    PROPERTY_VEC3("Scale", _scale, 0.0001f);

    return true;
}

namespace
{
    Client::Mon_Simon_Weapon* Resolve_SimonWeapon(const AnimNotifyContext& context, const wstring& partTag)
    {
        if (context.owner == nullptr)
            return nullptr;

        Shared<GameObject> current = context.owner->GetSharedPtr<GameObject>();
        while (current)
        {
            ContainerObject* ownerContainer = dynamic_cast<ContainerObject*>(current.get());
            if (ownerContainer != nullptr)
            {
                if (auto* weapon = dynamic_cast<Client::Mon_Simon_Weapon*>(ownerContainer->Get_PartObject(partTag)))
                    return weapon;
            }

            current = current->Get_Owner();
        }

        return nullptr;
    }
}

void AN_WeaponDetach::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (auto* weapon = Resolve_SimonWeapon(context, _partTag))
    {
        if (_applyScale)
        {
            if (const Shared<TransformCom> transform = weapon->Get_Transform())
                transform->Set_Scale(_scale);
        }

        weapon->Detach_PreserveWorld();
    }
}
