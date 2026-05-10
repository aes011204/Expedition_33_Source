#include "Level_EffectEditor.h"

#include "EffectEditorCamera.h"
#include "EffectEditorInstance.h"
#include "ComputePointParticlePreview.h"
#include "GameInstance.h"
#include "PointParticlePreview.h"

NS_BEGIN(EffectEditor)

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
    CHECK_FAILED(Ready_Layer_Preview(L"Layer_Preview"), E_FAIL);

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
    // TODO : 여기 있는 건 임시 파티클 들이므로 나중에 지울 것

    Client::PointParticlePreview::PointParticlePreviewDesc instancedIndirectDesc{};
    instancedIndirectDesc.drawMode = PointParticleDrawMode::DrawInstancedIndirect;
    instancedIndirectDesc.center = Vec3{ -2.f, 2.f, 0.f };

    const HRESULT instancedHr = GAME->Add_GameObject(
        ETOI(LevelType::GamePlay),
        L"GameObject_PointParticlePreview",
        ETOI(LevelType::GamePlay),
        layerTag,
        &instancedIndirectDesc
    );
    if (FAILED(instancedHr))
    {
        LOG_ERROR("Failed to add DrawInstancedIndirect PointParticlePreview in Level_EffectEditor");
        return E_FAIL;
    }

    Client::PointParticlePreview::PointParticlePreviewDesc indexedInstancedIndirectDesc{};
    indexedInstancedIndirectDesc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;
    indexedInstancedIndirectDesc.center = Vec3{ 2.f, 2.f, 0.f };

    const HRESULT indexedHr = GAME->Add_GameObject(
        ETOI(LevelType::GamePlay),
        L"GameObject_PointParticlePreview",
        ETOI(LevelType::GamePlay),
        layerTag,
        &indexedInstancedIndirectDesc
    );
    if (FAILED(indexedHr))
    {
        LOG_ERROR("Failed to add DrawIndexedInstancedIndirect PointParticlePreview in Level_EffectEditor");
        return E_FAIL;
    }

    Client::ComputePointParticlePreview::ComputePointParticlePreviewDesc computeDirectDesc{};
    computeDirectDesc.drawMode = PointParticleDrawMode::Direct;
    computeDirectDesc.center = Vec3{ 0.f, 2.f, 0.f };

    const HRESULT computeHr = GAME->Add_GameObject(
        ETOI(LevelType::GamePlay),
        L"GameObject_ComputePointParticlePreview",
        ETOI(LevelType::GamePlay),
        layerTag,
        &computeDirectDesc
    );
    if (FAILED(computeHr))
    {
        LOG_ERROR("Failed to add ComputePointParticlePreview in Level_EffectEditor");
        return E_FAIL;
    }

    Client::ComputePointParticlePreview::ComputePointParticlePreviewDesc computeInstancedIndirectDesc{};
    computeInstancedIndirectDesc.drawMode = PointParticleDrawMode::DrawInstancedIndirect;
    computeInstancedIndirectDesc.center = Vec3{ -4.f, 2.f, 0.f };

    const HRESULT computeInstancedHr = GAME->Add_GameObject(
        ETOI(LevelType::GamePlay),
        L"GameObject_ComputePointParticlePreview",
        ETOI(LevelType::GamePlay),
        layerTag,
        &computeInstancedIndirectDesc
    );
    if (FAILED(computeInstancedHr))
    {
        LOG_ERROR("Failed to add compute DrawInstancedIndirect preview in Level_EffectEditor");
        return E_FAIL;
    }

    Client::ComputePointParticlePreview::ComputePointParticlePreviewDesc computeIndexedIndirectDesc{};
    computeIndexedIndirectDesc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;
    computeIndexedIndirectDesc.center = Vec3{ 4.f, 2.f, 0.f };

    const HRESULT computeIndexedHr = GAME->Add_GameObject(
        ETOI(LevelType::GamePlay),
        L"GameObject_ComputePointParticlePreview",
        ETOI(LevelType::GamePlay),
        layerTag,
        &computeIndexedIndirectDesc
    );
    if (FAILED(computeIndexedHr))
    {
        LOG_ERROR("Failed to add compute DrawIndexedInstancedIndirect preview in Level_EffectEditor");
        return E_FAIL;
    }

    return S_OK;
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
    __super::Free();
}

NS_END
