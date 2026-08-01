#include "Compare_Culling_View.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "GameObject.h"
#include "TransformCom.h"
#include "ModelCom.h"
#include "MapStaticMesh.h"

NS_BEGIN(MainEditor)

Compare_Culling_View::Compare_Culling_View()
    : Editor_Window(L"Compare_Culling")
{
}

void Compare_Culling_View::Update(float timeDelta)
{
    if (_showStaticMeshCullBoxes)
        Draw_StaticMeshCullingBoxes();
}

void Compare_Culling_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    if (ImGui::Begin("Compare Culling View", &isOpen))
    {
        ImGui::Text("Instance Mesh");
        bool disableComputeCull = ClientInstance::Is_DebugDisableInstanceComputeCull();
        if (ImGui::Checkbox("Disable Compute Shader Culling", &disableComputeCull))
            ClientInstance::Set_DebugDisableInstanceComputeCull(disableComputeCull);

        ImGui::Separator();

        ImGui::Text("Static Mesh");
        ImGui::Checkbox("Show Static Mesh Culling Boxes", &_showStaticMeshCullBoxes);
    }
    ImGui::End();

    Set_Open(isOpen);
}

void Compare_Culling_View::Draw_StaticMeshCullingBoxes()
{
    GAME->Visit_LevelObjects(
        GAME->Current_LevelIndex(),
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (layerTag != L"Layer_MapStaticMesh" || gameObject == nullptr)
                return;

            const Shared<Client::MapStaticMesh> mapStaticMesh = dynamic_pointer_cast<Client::MapStaticMesh>(gameObject);
            if (mapStaticMesh == nullptr)
                return;

            const Shared<ModelCom> model = mapStaticMesh->Get_Model();
            const Shared<TransformCom> transform = gameObject->Get_Transform();
            if (model == nullptr || transform == nullptr)
                return;

            Vec3 localMin{}, localMax{};
            if (!model->Try_Get_LocalBounds(localMin, localMax))
                return;

            BoundingBox localBox;
            BoundingBox::CreateFromPoints(localBox, XMLoadFloat3(&localMin), XMLoadFloat3(&localMax));

            BoundingBox worldBox{};
            localBox.Transform(worldBox, model->Get_PreLocalTransformMatrix() * transform->Get_WorldMatrix());

            const bool visible = GAME->IsVisible(worldBox);

            DebugBoxDesc desc{};
            desc.center = worldBox.Center;
            desc.extents = worldBox.Extents;
            desc.style.color = visible ? Color(0.f, 1.f, 0.f, 1.f) : Color(1.f, 0.f, 0.f, 1.f);
            desc.style.depthEnabled = false;
            desc.style.duration = 1.0f;
            GAME->Draw_DebugBox(desc);
        });
}

Shared<Compare_Culling_View> Compare_Culling_View::Create()
{
    Shared<Compare_Culling_View> instance = make_shared<Compare_Culling_View>();
    if (FAILED(instance->Initialize()))
        MSG_BOX("Failed to Create : Compare_Culling_View");
    return instance;
}

NS_END
