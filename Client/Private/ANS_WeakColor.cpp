#include "ANS_WeakColor.h"

#include "ContainerObject.h"
#include "EffectCom.h"
#include "PartObject.h"

namespace
{
    bool Parse_EmitterIdToken(const string& token, uint32& outId)
    {
        if (token.empty())
            return false;

        try
        {
            size_t parsedLength = 0;
            const unsigned long value = stoul(token, &parsedLength, 10);
            if (parsedLength != token.size())
                return false;

            outId = static_cast<uint32>(value);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}

IMPLEMENT_REFLECTION(ANS_WeakColor)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Weak Part", _partTag);
    PROPERTY_STRING("Emitter Ids", _emitterIds);
    PROPERTY_COLOR("Target Color", _targetColor);
    PROPERTY_FLOAT("Strength", _strength, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("Fade In Ratio", _fadeInRatio, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("Fade Out Ratio", _fadeOutRatio, 0.f, 1.f, 0.01f);

    return true;
}

void ANS_WeakColor::On_Begin(const AnimNotifyContext& context)
{
    Apply_Color(context, context.notifyStateProgress);
}

void ANS_WeakColor::On_Tick(const AnimNotifyContext& context)
{
    Apply_Color(context, context.notifyStateProgress);
}

void ANS_WeakColor::On_End(const AnimNotifyContext& context)
{
    Clear_Color(context);
}

Shared<EffectCom> ANS_WeakColor::Resolve_WeakEffectCom(const AnimNotifyContext& context) const
{
    if (_partTag.empty())
        return EffectCom::Find_FromNotifyOwner(context.owner);

    Shared<GameObject> current = context.owner != nullptr ? context.owner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        const Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(current);
        if (container)
        {
            if (PartObject* partObject = container->Get_PartObject(_partTag))
            {
                if (Shared<EffectCom> effectCom = partObject->Get_Component<EffectCom>())
                    return effectCom;
            }
        }

        current = current->Get_Owner();
    }

    return nullptr;
}

vector<uint32> ANS_WeakColor::Resolve_EmitterIds() const
{
    vector<uint32> emitterIds{};
    string token{};

    auto flushToken = [&]()
    {
        uint32 emitterId = 0;
        if (Parse_EmitterIdToken(token, emitterId))
            emitterIds.push_back(emitterId);
        token.clear();
    };

    for (const char ch : _emitterIds)
    {
        if (ch == ',' || ch == ';' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
        {
            flushToken();
            continue;
        }

        token.push_back(ch);
    }

    flushToken();
    return emitterIds;
}

float ANS_WeakColor::Resolve_BlendStrength(float progress) const
{
    const float clampedProgress = clamp(progress, 0.f, 1.f);
    const float fadeInRatio = clamp(_fadeInRatio, 0.f, 1.f);
    const float fadeOutRatio = clamp(_fadeOutRatio, 0.f, 1.f);

    float blend = 1.f;
    if (fadeInRatio > 0.f)
        blend = min(blend, clampedProgress / fadeInRatio);
    if (fadeOutRatio > 0.f)
        blend = min(blend, (1.f - clampedProgress) / fadeOutRatio);

    return clamp(blend, 0.f, 1.f) * clamp(_strength, 0.f, 1.f);
}

void ANS_WeakColor::Apply_Color(const AnimNotifyContext& context, float progress) const
{
    const Shared<EffectCom> effectCom = Resolve_WeakEffectCom(context);
    if (effectCom == nullptr)
        return;

    effectCom->Set_AttachedEffectMaterialTintOverride(
        Resolve_EmitterIds(),
        _targetColor,
        Resolve_BlendStrength(progress));
}

void ANS_WeakColor::Clear_Color(const AnimNotifyContext& context) const
{
    const Shared<EffectCom> effectCom = Resolve_WeakEffectCom(context);
    if (effectCom == nullptr)
        return;

    effectCom->Set_AttachedEffectMaterialTintOverride(Resolve_EmitterIds(), _targetColor, 0.f);
}
