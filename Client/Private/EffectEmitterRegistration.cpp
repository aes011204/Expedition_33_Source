#include "EffectEmitterRegistration.h"

#include "ComputeSpriteEmitter.h"
#include "ComputeTrailEmitter.h"
#include "ComputeRibbonEmitter.h"
#include "ComputeSourceHistoryRibbonEmitter.h"
#include "EffectEmitterFactory.h"
#include "GameInstance.h"

NS_BEGIN(Client)

namespace Internal
{
    constexpr const wchar_t* kComputeRibbonEmitterPrototypeTag = L"GameObject_Effect_ComputeRibbonEmitter";
    constexpr const wchar_t* kComputeSourceHistoryRibbonEmitterPrototypeTag = L"GameObject_Effect_ComputeSourceHistoryRibbonEmitter";

    Shared<EffectEmitter> Create_ClientEffectEmitter(const EffectEmitterDefinition& emitterDefinition)
    {
        switch (emitterDefinition.kind)
        {
        case EffectEmitterKind::ComputeSprite:
        {
            const auto* desc = get_if<ComputeSpriteEmitterDesc>(&emitterDefinition.concreteDesc);
            if (nullptr == desc)
                return nullptr;

            ComputeSpriteEmitterDesc concreteDesc = *desc;
            concreteDesc.renderLayerOverride = emitterDefinition.renderLayerOverride;
            const Shared<GameObject> clonedObject = dynamic_pointer_cast<GameObject>(
                GAME->Clone_Prototype(
                    Prototype::GameObject,
                    ETOI(LevelType::Static),
                    L"GameObject_Effect_ComputeSpriteEmitter",
                    &concreteDesc
                )
            );

            return dynamic_pointer_cast<EffectEmitter>(clonedObject);
        }

        case EffectEmitterKind::ComputeTrail:
        {
            const auto* desc = get_if<ComputeTrailEmitterDesc>(&emitterDefinition.concreteDesc);
            if (nullptr == desc)
                return nullptr;

            ComputeTrailEmitterDesc concreteDesc = *desc;
            concreteDesc.renderLayerOverride = emitterDefinition.renderLayerOverride;
            const Shared<GameObject> clonedObject = dynamic_pointer_cast<GameObject>(
                GAME->Clone_Prototype(
                    Prototype::GameObject,
                    ETOI(LevelType::Static),
                    L"GameObject_Effect_ComputeTrailEmitter",
                    &concreteDesc
                )
            );

            return dynamic_pointer_cast<EffectEmitter>(clonedObject);
        }

        case EffectEmitterKind::ComputeRibbon:
        {
            const auto* desc = get_if<ComputeRibbonEmitterDesc>(&emitterDefinition.concreteDesc);
            if (nullptr == desc)
                return nullptr;

            ComputeRibbonEmitterDesc concreteDesc = *desc;
            concreteDesc.renderLayerOverride = emitterDefinition.renderLayerOverride;
            const Shared<GameObject> clonedObject = dynamic_pointer_cast<GameObject>(
                GAME->Clone_Prototype(
                    Prototype::GameObject,
                    ETOI(LevelType::Static),
                    kComputeRibbonEmitterPrototypeTag,
                    &concreteDesc
                )
            );
            if (clonedObject == nullptr)
            {
                LOG_WARN(
                    "ComputeRibbonEmitter prototype missing: failed to clone ribbon emitter prototype. emitter='{}'",
                    emitterDefinition.name
                );
            }

            return dynamic_pointer_cast<EffectEmitter>(clonedObject);
        }

        case EffectEmitterKind::SourceHistoryRibbon:
        {
            const auto* desc = get_if<ComputeSourceHistoryRibbonEmitterDesc>(&emitterDefinition.concreteDesc);
            if (nullptr == desc)
                return nullptr;

            ComputeSourceHistoryRibbonEmitterDesc concreteDesc = *desc;
            concreteDesc.renderLayerOverride = emitterDefinition.renderLayerOverride;
            const Shared<GameObject> clonedObject = dynamic_pointer_cast<GameObject>(
                GAME->Clone_Prototype(
                    Prototype::GameObject,
                    ETOI(LevelType::Static),
                    kComputeSourceHistoryRibbonEmitterPrototypeTag,
                    &concreteDesc
                )
            );
            if (clonedObject == nullptr)
            {
                LOG_WARN(
                    "ComputeSourceHistoryRibbonEmitter prototype missing: failed to clone source history ribbon emitter prototype. emitter='{}'",
                    emitterDefinition.name
                );
            }

            return dynamic_pointer_cast<EffectEmitter>(clonedObject);
        }

        case EffectEmitterKind::Mesh:
        {
            const auto* desc = get_if<MeshEmitterDesc>(&emitterDefinition.concreteDesc);
            if (nullptr == desc)
                return nullptr;

            MeshEmitterDesc concreteDesc = *desc;
            concreteDesc.renderLayerOverride = emitterDefinition.renderLayerOverride;
            const Shared<GameObject> clonedObject = dynamic_pointer_cast<GameObject>(
                GAME->Clone_Prototype(
                    Prototype::GameObject,
                    ETOI(LevelType::Static),
                    L"GameObject_Effect_MeshEmitter",
                    &concreteDesc
                )
            );
            if (clonedObject == nullptr)
            {
                LOG_WARN(
                    "MeshEmitter prototype missing: failed to clone mesh emitter prototype. emitter='{}'",
                    emitterDefinition.name
                );
            }

            return dynamic_pointer_cast<EffectEmitter>(clonedObject);
        }

        default:
            return nullptr;
        }
    }
}

void Register_ClientEffectEmitters()
{
    EffectEmitterFactory::Register_CreateEmitterFn(&Internal::Create_ClientEffectEmitter);
}

NS_END
