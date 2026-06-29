#include "SL_Base.h"

SL_Base::SL_Base(ComPtr<Device> device, ComPtr<Context> context)
    : Character_WorldBase(device, context)
{
}

SL_Base::SL_Base(const SL_Base& prototype)
    : Character_WorldBase(prototype)
{
}

void SL_Base::Set_AttackHitAnimOverride(const string& colliderTag, SL_HitAnimOverride hitAnimOverride)
{
    UNREFERENCED_PARAMETER(colliderTag);
    UNREFERENCED_PARAMETER(hitAnimOverride);
}

void SL_Base::Clear_AttackHitAnimOverride(const string& colliderTag)
{
    UNREFERENCED_PARAMETER(colliderTag);
}

void SL_Base::Set_AttackHitEffect(const string& colliderTag, const wstring& effectName, const Vec3& localOffset, const Vec3& scale)
{
    UNREFERENCED_PARAMETER(colliderTag);
    UNREFERENCED_PARAMETER(effectName);
    UNREFERENCED_PARAMETER(localOffset);
    UNREFERENCED_PARAMETER(scale);
}

void SL_Base::Clear_AttackHitEffect(const string& colliderTag)
{
    UNREFERENCED_PARAMETER(colliderTag);
}
