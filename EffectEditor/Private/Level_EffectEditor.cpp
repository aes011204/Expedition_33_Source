#include "Level_EffectEditor.h"

#include "EffectEditorCamera.h"
#include "EffectEditorInstance.h"
#include "Emitter_View.h"
#include "GameInstance.h"
#include "PointParticlePreview.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    template <typename TData>
    const TData* Find_ModuleData(const AuthoringEmitter& emitter, AuthoringModuleType type)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != type)
                continue;

            if (!module.enabled)
                continue;

            return get_if<TData>(&module.data);
        }

        return nullptr;
    }

    float Evaluate_FloatDistributionMin(const FloatDistributionData& data, float fallbackValue)
    {
        if (const auto* constant = get_if<ConstantFloatDistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformFloatDistributionData>(&data.payload))
            return uniform->minValue;

        return fallbackValue;
    }

    float Evaluate_FloatDistributionMax(const FloatDistributionData& data, float fallbackValue)
    {
        if (const auto* constant = get_if<ConstantFloatDistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformFloatDistributionData>(&data.payload))
            return uniform->maxValue;

        return fallbackValue;
    }

    Vec2 Evaluate_Vector2DistributionMin(const Vector2DistributionData& data, const Vec2& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantVector2DistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformVector2DistributionData>(&data.payload))
            return uniform->minValue;

        return fallbackValue;
    }

    Vec2 Evaluate_Vector2DistributionMax(const Vector2DistributionData& data, const Vec2& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantVector2DistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformVector2DistributionData>(&data.payload))
            return uniform->maxValue;

        return fallbackValue;
    }

    Color Clamp_PreviewColor(Color value)
    {
        value.R(clamp(value.R(), 0.f, 1.f));
        value.G(clamp(value.G(), 0.f, 1.f));
        value.B(clamp(value.B(), 0.f, 1.f));
        value.A(clamp(value.A(), 0.f, 1.f));
        return value;
    }

    Color Evaluate_ColorDistributionMin(const ColorDistributionData& data, const Color& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(constant->value);

        if (const auto* uniform = get_if<UniformColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(uniform->minValue);

        return Clamp_PreviewColor(fallbackValue);
    }

    Color Evaluate_ColorDistributionMax(const ColorDistributionData& data, const Color& fallbackValue)
    {
        if (const auto* constant = get_if<ConstantColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(constant->value);

        if (const auto* uniform = get_if<UniformColorDistributionData>(&data.payload))
            return Clamp_PreviewColor(uniform->maxValue);

        return Clamp_PreviewColor(fallbackValue);
    }

    Client::PointParticlePreview::PointParticlePreviewDesc Build_PointParticlePreviewDesc(const AuthoringEmitter& emitter)
    {
        Client::PointParticlePreview::PointParticlePreviewDesc desc{};
        desc.drawMode = PointParticleDrawMode::Direct;

        if (const auto* required = Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
        {
            desc.center = required->emitterOrigin;
            desc.loop = required->loopCount == 0;
        }

        if (const auto* spawn = Find_ModuleData<SpawnModuleData>(emitter, AuthoringModuleType::Spawn))
            desc.instanceCount = max(1u, spawn->maxParticleCount);

        if (const auto* initialSize = Find_ModuleData<InitialSizeModuleData>(emitter, AuthoringModuleType::InitialSize))
        {
            const Vec2 minScale = Evaluate_Vector2DistributionMin(initialSize->size, desc.scale);
            const Vec2 maxScale = Evaluate_Vector2DistributionMax(initialSize->size, desc.scale);
            desc.scale = Vec2{ min(minScale.x, maxScale.x), max(minScale.y, maxScale.y) };
        }

        if (const auto* initialVelocity = Find_ModuleData<InitialVelocityModuleData>(emitter, AuthoringModuleType::InitialVelocity))
        {
            desc.pivot = initialVelocity->radialPivot;
            desc.speed = Vec2{
                Evaluate_FloatDistributionMin(initialVelocity->speed, desc.speed.x),
                Evaluate_FloatDistributionMax(initialVelocity->speed, desc.speed.y)
            };
        }
        else
            desc.pivot = desc.center;

        if (const auto* lifetime = Find_ModuleData<LifetimeModuleData>(emitter, AuthoringModuleType::Lifetime))
        {
            desc.lifeTime = Vec2{
                Evaluate_FloatDistributionMin(lifetime->lifeTime, desc.lifeTime.x),
                Evaluate_FloatDistributionMax(lifetime->lifeTime, desc.lifeTime.y)
            };
        }

        if (const auto* initialColor = Find_ModuleData<InitialColorModuleData>(emitter, AuthoringModuleType::InitialColor))
        {
            const Color minColor = Evaluate_ColorDistributionMin(
                initialColor->color,
                Color{ desc.startColorMin.x, desc.startColorMin.y, desc.startColorMin.z, desc.startColorMin.w }
            );
            const Color maxColor = Evaluate_ColorDistributionMax(
                initialColor->color,
                Color{ desc.startColorMax.x, desc.startColorMax.y, desc.startColorMax.z, desc.startColorMax.w }
            );

            desc.startColorMin = Vec4{ minColor.R(), minColor.G(), minColor.B(), minColor.A() };
            desc.startColorMax = Vec4{ maxColor.R(), maxColor.G(), maxColor.B(), maxColor.A() };
            desc.endColorMin = desc.startColorMin;
            desc.endColorMax = desc.startColorMax;
        }

        if (const auto* colorOverLife = Find_ModuleData<ColorOverLifeModuleData>(emitter, AuthoringModuleType::ColorOverLife))
        {
            const Color minEndColor = Evaluate_ColorDistributionMin(
                colorOverLife->colorOverLife,
                Color{ desc.endColorMin.x, desc.endColorMin.y, desc.endColorMin.z, desc.endColorMin.w }
            );
            const Color maxEndColor = Evaluate_ColorDistributionMax(
                colorOverLife->colorOverLife,
                Color{ desc.endColorMax.x, desc.endColorMax.y, desc.endColorMax.z, desc.endColorMax.w }
            );

            desc.endColorMin = Vec4{ minEndColor.R(), minEndColor.G(), minEndColor.B(), minEndColor.A() };
            desc.endColorMax = Vec4{ maxEndColor.R(), maxEndColor.G(), maxEndColor.B(), maxEndColor.A() };
        }

        return desc;
    }
}

Level_EffectEditor::Level_EffectEditor(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

Level_EffectEditor::~Level_EffectEditor()
{
    Free();
}

HRESULT Level_EffectEditor::Initialize()
{
    CHECK_FAILED(Level::Initialize(), E_FAIL);
    CHECK_FAILED(Ready_Lights(), E_FAIL);
    CHECK_FAILED(Ready_Layer_Camera(L"Layer_Camera"), E_FAIL);
    CHECK_FAILED(Ready_Layer_Preview(kPreviewLayerTag), E_FAIL);

    return S_OK;
}

void Level_EffectEditor::Update(float timeDelta)
{
    __super::Update(timeDelta);
}

HRESULT Level_EffectEditor::Render()
{
    CHECK_FAILED(__super::Render(), E_FAIL);

    return S_OK;
}

HRESULT Level_EffectEditor::Restart_PreviewFromAuthoring()
{
    Clear_PreviewObjects();

    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    const Shared<Emitter_View> emitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
    if (nullptr == emitterView)
    {
        LOG_ERROR("Emitter view is not available for preview restart.");
        return E_FAIL;
    }

    vector<Shared<GameObject>> restartedPreviewObjects{};
    vector<uint32> restartedEmitterIds{};
    for (const AuthoringEmitter& emitter : emitterView->Get_Emitters())
    {
        if (!emitter.enabled)
            continue;

        Client::PointParticlePreview::PointParticlePreviewDesc previewDesc = Internal::Build_PointParticlePreviewDesc(emitter);
        Shared<GameObject> previewObject{};
        if (FAILED(
            GAME->Add_GameObject(
                ETOI(LevelType::GamePlay),
                L"GameObject_PointParticlePreview",
                ETOI(LevelType::GamePlay),
                kPreviewLayerTag,
                previewObject,
                &previewDesc
            )
        ))
        {
            for (Shared<GameObject>& createdPreviewObject : restartedPreviewObjects)
                GAME->Remove_GameObject(ETOI(LevelType::GamePlay), createdPreviewObject);

            return E_FAIL;
        }

        restartedPreviewObjects.push_back(previewObject);
        restartedEmitterIds.push_back(emitter.id);
    }

    _previewObjects = move(restartedPreviewObjects);

    for (const uint32 emitterId : restartedEmitterIds)
        emitterView->Clear_PreviewDirty(emitterId);

    return S_OK;
}

HRESULT Level_EffectEditor::Ready_Lights()
{
    LIGHT_DESC lightDesc{};
    lightDesc.type = LightType::Directional;
    lightDesc.direction = Vec4(1.f, -1.f, 1.f, 0.f);
    lightDesc.direction.Normalize();
    lightDesc.diffuse = Vec4(1.f, 1.f, 1.f, 1.f);
    lightDesc.ambient = Vec4(0.6f, 0.6f, 0.6f, 1.f);
    lightDesc.specular = Vec4(1.f, 1.f, 1.f, 1.f);

    CHECK_FAILED(GAME->Add_Light(lightDesc), E_FAIL);

    return S_OK;
}

HRESULT Level_EffectEditor::Ready_Layer_Camera(const wstring& layerTag)
{
    EffectEditorCamera::EffectEditorCameraDesc cameraDesc{};
    cameraDesc.speedPerSec = 10.f;
    cameraDesc.degreePerSec = 180.f;
    cameraDesc.eye = Vec4(0.f, 8.f, -8.f, 1.f);
    cameraDesc.at = Vec4(0.f, 0.f, 0.f, 1.f);
    cameraDesc.fovy = XMConvertToRadians(60.f);
    cameraDesc.nearPlane = 0.1f;
    cameraDesc.farPlane = 500.f;
    cameraDesc.mouseSensor = 0.05f;
    cameraDesc.yaw = 0.f;
    cameraDesc.pitch = 0.f;
    cameraDesc.pivot = Vec3(0.f, 0.f, 0.f);
    cameraDesc.focusSpeed = 10.f;

    const Shared<EffectEditorCamera> previewCamera =
        EffectEditorCamera::Create(_device, _context);
    CHECK_NULL(previewCamera, E_FAIL);
    CHECK_FAILED(previewCamera->Initialize(&cameraDesc), E_FAIL);
    EDITOR->Register_PreviewCamera(previewCamera);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), layerTag, previewCamera), E_FAIL);

    GAME->Change_ActiveCamera(previewCamera);

    return S_OK;
}

HRESULT Level_EffectEditor::Ready_Layer_Preview(const wstring& layerTag)
{
    if (layerTag.empty())
        return E_FAIL;

    return S_OK;
}

void Level_EffectEditor::Clear_PreviewObjects()
{
    for (Shared<GameObject>& previewObject : _previewObjects)
    {
        if (previewObject != nullptr)
            GAME->Remove_GameObject(ETOI(LevelType::GamePlay), previewObject);
    }

    _previewObjects.clear();
}

Shared<Level_EffectEditor> Level_EffectEditor::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Level_EffectEditor>(device, context);

    if (FAILED(instance->Initialize()))
    {
        LOG_CRITICAL("Failed to Create : Level_EffectEditor");
        return nullptr;
    }

    return instance;
}

void Level_EffectEditor::Free()
{
    Clear_PreviewObjects();
    __super::Free();
}

NS_END
