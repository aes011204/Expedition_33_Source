#include "Level_EffectEditor.h"

#include "EffectEditorCamera.h"
#include "EffectEditorInstance.h"
#include "EffectInstance.h"
#include "Emitter_View.h"
#include "GameInstance.h"

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

    float Evaluate_FloatCurveSegmentLinear(const FloatCurveKeyData& leftKey, const FloatCurveKeyData& rightKey, float x)
    {
        const float width = max(0.0001f, rightKey.time - leftKey.time);
        const float t = clamp((x - leftKey.time) / width, 0.f, 1.f);
        return lerp(leftKey.value, rightKey.value, t);
    }

    float Evaluate_FloatCurveSegmentAutoClamped(const FloatCurveKeyData& leftKey, const FloatCurveKeyData& rightKey, float x)
    {
        const float width = max(0.0001f, rightKey.time - leftKey.time);
        const float t = clamp((x - leftKey.time) / width, 0.f, 1.f);
        const float m0 = leftKey.leaveTangent * width;
        const float m1 = rightKey.arriveTangent * width;
        const float t2 = t * t;
        const float t3 = t2 * t;
        return (2.f * t3 - 3.f * t2 + 1.f) * leftKey.value +
               (t3 - 2.f * t2 + t) * m0 +
               (-2.f * t3 + 3.f * t2) * rightKey.value +
               (t3 - t2) * m1;
    }

    float Evaluate_FloatConstantCurve(const ConstantCurveFloatDistributionData& curve, float x, float fallbackValue)
    {
        if (curve.keys.empty())
            return fallbackValue;

        if (curve.keys.size() == 1)
            return curve.keys.front().value;

        const float clampedX = clamp(x, 0.f, 1.f);
        for (size_t index = 1; index < curve.keys.size(); ++index)
        {
            const FloatCurveKeyData& leftKey = curve.keys[index - 1];
            const FloatCurveKeyData& rightKey = curve.keys[index];
            if (clampedX > rightKey.time)
                continue;

            switch (leftKey.interpolationMode)
            {
            case FloatCurveInterpolationMode::Constant:
                return leftKey.value;

            case FloatCurveInterpolationMode::CurveAutoClamped:
                return Evaluate_FloatCurveSegmentAutoClamped(leftKey, rightKey, clampedX);

            case FloatCurveInterpolationMode::Linear:
            default:
                return Evaluate_FloatCurveSegmentLinear(leftKey, rightKey, clampedX);
            }
        }

        return curve.keys.back().value;
    }

    float Evaluate_FloatDistributionMin(const FloatDistributionData& data, float fallbackValue)
    {
        if (const auto* constant = get_if<ConstantFloatDistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformFloatDistributionData>(&data.payload))
            return uniform->minValue;

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.payload))
            return Evaluate_FloatConstantCurve(*curve, 0.f, fallbackValue);

        return fallbackValue;
    }

    float Evaluate_FloatDistributionMax(const FloatDistributionData& data, float fallbackValue)
    {
        if (const auto* constant = get_if<ConstantFloatDistributionData>(&data.payload))
            return constant->value;

        if (const auto* uniform = get_if<UniformFloatDistributionData>(&data.payload))
            return uniform->maxValue;

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.payload))
            return Evaluate_FloatConstantCurve(*curve, 1.f, fallbackValue);

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

    uint32 Resolve_SubUVFrameCount(const RequiredModuleData& required)
    {
        return max(1u, max(1u, required.material.subUVRows) * max(1u, required.material.subUVCols));
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

    const Shared<const EffectDefinition> previewDefinition =
        Build_PreviewEffectDefinition(emitterView->Get_Emitters());
    CHECK_NULL(previewDefinition, E_FAIL);

    EffectInstanceDesc previewDesc{};
    previewDesc.definition = previewDefinition;

    const Shared<EffectInstance> previewPrototype = EffectInstance::Create(_device, _context);
    CHECK_NULL(previewPrototype, E_FAIL);

    const Shared<GameObject> previewObject = previewPrototype->Clone(&previewDesc);
    CHECK_NULL(previewObject, E_FAIL);

    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), kPreviewLayerTag, previewObject), E_FAIL);

    vector<Shared<GameObject>> restartedPreviewObjects{};
    restartedPreviewObjects.push_back(previewObject);

    _previewObjects = move(restartedPreviewObjects);

    for (const AuthoringEmitter& emitter : emitterView->Get_Emitters())
    {
        if (emitter.enabled)
            emitterView->Clear_PreviewDirty(emitter.id);
    }

    return S_OK;
}

Shared<const EffectDefinition> Level_EffectEditor::Build_PreviewEffectDefinition(const vector<AuthoringEmitter>& emitters) const
{
    auto definition = make_shared<EffectDefinition>();
    definition->name = "EffectEditorPreview";

    for (const AuthoringEmitter& emitter : emitters)
    {
        if (!emitter.enabled)
            continue;

        const string rendererTypeLower = String::ToLowerCopy(emitter.rendererType);
        if (rendererTypeLower != "sprite")
        {
            LOG_INFO("EffectEditor preview skip: emitter='{}', rendererType='{}'", emitter.name, emitter.rendererType);
            continue;
        }

        definition->emitters.push_back(Build_PreviewEmitterDefinition(emitter));
    }

    return definition;
}

EffectEmitterDefinition Level_EffectEditor::Build_PreviewEmitterDefinition(const AuthoringEmitter& emitter) const
{
    EffectEmitterDefinition definition{};
    definition.id = emitter.id;
    definition.name = emitter.name;
    definition.kind = EffectEmitterKind::ComputeSprite;
    definition.enabled = emitter.enabled;

    ComputeSpriteEmitterDesc desc{};
    desc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;

    if (const auto* required = Internal::Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required))
    {
        const EffectMaterialInstanceData& material = required->material;

        if (!material.mainTextureId.empty())
            desc.textureId = material.mainTextureId;

        if (!material.blendMode.empty())
            desc.blendMode = material.blendMode;

        if (!material.opacitySource.empty())
            desc.opacitySource = material.opacitySource;

        desc.tint = material.tint;
        desc.intensity = material.intensity;
        desc.opacityPower = material.opacityPower;

        if (!material.noiseTextureId.empty())
            desc.noiseTextureId = material.noiseTextureId;

        desc.noiseStrength = material.noiseStrength;
        desc.subUVRows = max(1u, material.subUVRows);
        desc.subUVCols = max(1u, material.subUVCols);

        definition.localPosition = required->emitterOrigin;
        definition.localRotationDegrees = required->emitterRotationDegrees;
        definition.useLocalSpace = required->useLocalSpace;
    }

    definition.concreteDesc = desc;
    return definition;
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
    cameraDesc.mouseSensor = 0.065f;
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
