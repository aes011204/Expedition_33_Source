#include "Prefab_View.h"

#include "EditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Component.h"
#include "ContainerObject.h"
#include "PartObject.h"
#include "Editor_Camera_Free.h"
#include "Inspector.h"
#include "Helper_String.h"
#include "Animation_View.h"
#include "ModelCom.h"
#include "RenderTarget.h"
#include "imgui_internal.h"
#include <algorithm>

Prefab_View::Prefab_View()
    : Editor_Window(L"Prefab")
{
    Set_Open(false);
}

void Prefab_View::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);

    if (Is_AnimationViewFocused())
        return;

    if (_isPreviewHovered && _previewCamera)
        _previewCamera->Priority_Update(timeDelta);
}

void Prefab_View::Pre_Render()
{
    if (!Is_Open())
        return;

    if (Is_AnimationViewFocused())
        return;

    if (_previewObject == nullptr || _previewCamera == nullptr)
        return;

    Ensure_PreviewRenderTarget(_previewWidth, _previewHeight);
    Ensure_PreviewGridResources();

    if (_previewRenderTarget == nullptr)
        return;

    if (!_previewHasBegunPlay)
    {
        _previewObject->Try_BeginPlay();
        _previewHasBegunPlay = true;
    }

    const Matrix savedView = *GAME->Get_Transform(D3DTS::View);
    const Matrix savedProj = *GAME->Get_Transform(D3DTS::Proj);
    vector<LIGHT_DESC> savedLightDescs{};
    GAME->Get_LightDescs(savedLightDescs);
    const bool wasPlayerInputBlocked = GAME->Is_PlayerInputBlocked();

    if (FAILED(_previewRenderTarget->Begin(true, true)))
        return;

    const Matrix previewView = _previewCamera->Get_ViewMatrix();
    const float aspect = static_cast<float>(_previewWidth) / static_cast<float>(max(1u, _previewHeight));
    const Matrix previewProj = XMMatrixPerspectiveFovLH(_previewCamera->Get_FovY(), aspect, _previewCamera->Get_Near(), _previewCamera->Get_Far());

    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(previewView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(previewProj));

    vector<LIGHT_DESC> previewLightDescs{};
    previewLightDescs.reserve(2);

    LIGHT_DESC keyLight{};
    keyLight.type = LightType::Directional;
    keyLight.direction = Vec4(0.35f, -1.f, 0.45f, 0.f);
    keyLight.diffuse = Vec4(0.55f, 0.55f, 0.55f, 1.f);
    keyLight.ambient = Vec4(0.14f, 0.14f, 0.14f, 1.f);
    keyLight.specular = Vec4(0.18f, 0.18f, 0.18f, 1.f);
    previewLightDescs.push_back(keyLight);

    LIGHT_DESC fillLight{};
    fillLight.type = LightType::Directional;
    fillLight.direction = Vec4(-0.4f, -0.25f, -0.8f, 0.f);
    fillLight.diffuse = Vec4(0.18f, 0.18f, 0.2f, 1.f);
    fillLight.ambient = Vec4(0.04f, 0.04f, 0.04f, 1.f);
    fillLight.specular = Vec4(0.05f, 0.05f, 0.05f, 1.f);
    previewLightDescs.push_back(fillLight);

    GAME->Set_Lights(previewLightDescs);

    const float dt = ImGui::GetIO().DeltaTime;
    GAME->Set_PlayerInputBlocked(true);

    _previewObject->Priority_Update(dt);
    _previewObject->Update(dt);
    _previewObject->Late_Update(dt);
    Lock_PreviewObjectToOrigin();

    if (_requestResetPreviewCamera)
    {
        Reset_PreviewCameraPose();
        _requestResetPreviewCamera = false;
    }

    GAME->Draw_RendererOnly();
    Draw_PreviewGrid();
    GAME->Set_Lights(savedLightDescs);
    GAME->Set_PlayerInputBlocked(wasPlayerInputBlocked);

    GAME->Bind_BackBuffer();

    GAME->Set_Transform(D3DTS::View, const_cast<Matrix&>(savedView));
    GAME->Set_Transform(D3DTS::Proj, const_cast<Matrix&>(savedProj));
}

void Prefab_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    string title = windowName;
    if (!_prefabName.empty())
        title += " - " + _prefabName;

    if (Is_Dirty())
        title += " *";

    if (_openAnimationViewWithPrefab)
        Prepare_AnimationViewSiblingDock();

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (_pendingDockId != 0)
        ImGui::SetNextWindowDockID(_pendingDockId, ImGuiCond_Always);

    if (_focusPrefabFrames > 0)
        ImGui::SetNextWindowFocus();

    if (_applyDefaultWindowLayout && _pendingDockId == 0)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 defaultSize = ImVec2(1500.f, 920.f);

        ImGui::SetNextWindowSize(defaultSize, ImGuiCond_Always);

        if (viewport)
        {
            ImGui::SetNextWindowPos(
                viewport->GetCenter(),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
        }
    }

    _isFocused = false;
    _isHovered = false;

    if (ImGui::Begin(title.c_str(), &isOpen, windowFlags))
    {
        _applyDefaultWindowLayout = false;
        if (_pendingDockId != 0 || _focusPrefabFrames > 0)
            ImGui::SetWindowFocus();

        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        _dockId = ImGui::GetWindowDockID();
        _pendingDockId = 0;
        if (_focusPrefabFrames > 0)
            --_focusPrefabFrames;

        Draw_Header();
        ImGui::Separator();

        if (ImGui::BeginTable("PrefabViewLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Structure", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch, 2.2f);
            ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch, 1.4f);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            Draw_StructurePanel();

            ImGui::TableSetColumnIndex(1);
            Draw_PreviewPanel();

            ImGui::TableSetColumnIndex(2);
            Draw_InspectorPanel();

            ImGui::EndTable();
        }
    }

    Set_Open(isOpen);
    ImGui::End();

    if (_focusPrefabFrames > 0)
        ImGui::SetWindowFocus(title.c_str());

    if (_pendingOpenAnimationView)
        Open_PendingAnimationView();
}

bool Prefab_View::CanSave() const
{
    return _previewObject != nullptr && !_prefabPath.empty();
}

void Prefab_View::Save()
{
    if (!CanSave())
        return;

    if (SUCCEEDED(GAME->Save_GameObjectJson(_prefabPath, _previewObject)))
        ClearDirty();
}

void Prefab_View::Open_Prefab(const string& prefabName, const fs::path& prefabPath)
{
    _prefabName = prefabName;
    _prefabPath = prefabPath;
    _sourceStaticClass.clear();
    _applyDefaultWindowLayout = true;
    _openAnimationViewWithPrefab = true;

    _previewObject = GAME->Instantiate_GameObjectJson(prefabPath);
    _previewCamera = nullptr;
    _previewHasBegunPlay = false;
    _requestResetPreviewCamera = false;

    if (_previewObject)
    {
        Disable_PreviewAnimNotifyExecution(_previewObject);
        _sourceStaticClass = _previewObject->Get_ReflectionInfo().className;

        if (_sourceStaticClass.empty())
            _sourceStaticClass = String::ToString(_previewObject->Get_Name());

        _previewCamera = Editor_Camera_Free::Create(GAME->Get_Device(), GAME->Get_Context());

        if (_previewCamera)
        {
            Editor_Camera_Free::EDITOR_CAMERA_DESC desc = Build_PreviewCameraDesc(_previewObject);

            if (FAILED(_previewCamera->Initialize(&desc)))
                _previewCamera = nullptr;
            else
            {
                _previewCamera->Apply_EditorDesc(desc);
                Apply_PreviewCameraTransform(_previewCamera);
            }
        }

        Lock_PreviewObjectToOrigin();
        Ensure_PreviewGridResources();
        _requestResetPreviewCamera = true;
    }

    _selectionType = SelectionType::GameObject;
    _selectedObjectKey = L"Root";
    _selectedComponentKey.clear();
    _showPrefabCollider = false;

    ClearDirty();
    Set_Open(true);
}

void Prefab_View::Close_Prefab()
{
    _prefabName.clear();
    _prefabPath.clear();
    _sourceStaticClass.clear();

    _previewObject = nullptr;
    _previewCamera = nullptr;
    _previewRenderTarget = nullptr;
    _previewGridBatch.reset();
    _previewGridEffect.reset();
    _previewGridInputLayout.Reset();
    _previewGridDepthState.Reset();

    _previewWidth = 512;
    _previewHeight = 512;
    _previewHasBegunPlay = false;
    _requestResetPreviewCamera = false;
    _isPreviewHovered = false;
    _isPreviewActive = false;
    _applyDefaultWindowLayout = false;
    _openAnimationViewWithPrefab = false;
    _pendingOpenAnimationView = false;
    _dockId = 0;
    _pendingDockId = 0;
    _pendingAnimationDockId = 0;
    _focusPrefabFrames = 0;

    _selectionType = SelectionType::None;
    _selectedObjectKey.clear();
    _selectedComponentKey.clear();
    _showPrefabCollider = false;

    ClearDirty();
    Set_Open(false);
}

bool Prefab_View::Is_PreviewPrefab(const string& prefabName) const
{
    return Is_Open() && _prefabName == prefabName;
}

Shared<ModelCom> Prefab_View::Find_FirstModelComponent() const
{
    function<Shared<ModelCom>(const Shared<GameObject>&)> findRecursive =
        [&](const Shared<GameObject>& gameObject) -> Shared<ModelCom>
    {
        if (gameObject == nullptr)
            return nullptr;

        Shared<ModelCom> foundModel = nullptr;
        gameObject->Visit_Components([&](const wstring&, const Shared<Component>& component)
        {
            if (foundModel == nullptr)
                foundModel = dynamic_pointer_cast<ModelCom>(component);
        });

        if (foundModel != nullptr)
            return foundModel;

        Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
        if (container == nullptr)
            return nullptr;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);

            if (Shared<ModelCom> partModel = findRecursive(partObject))
                return partModel;
        }

        return nullptr;
    };

    return findRecursive(_previewObject);
}

void Prefab_View::Prepare_AnimationViewSiblingDock()
{
    _openAnimationViewWithPrefab = false;

    Shared<ModelCom> modelCom = Find_FirstModelComponent();
    if (modelCom == nullptr)
        return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 defaultSize = ImVec2(1500.f, 920.f);
    const ImVec2 defaultPos = viewport
        ? ImVec2(viewport->GetCenter().x - defaultSize.x * 0.5f, viewport->GetCenter().y - defaultSize.y * 0.5f)
        : ImVec2(80.f, 80.f);

    const ImGuiID dockId = ImHashStr(("PrefabAnimationDock_" + _prefabName).c_str());
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_None);
    ImGui::DockBuilderSetNodePos(dockId, defaultPos);
    ImGui::DockBuilderSetNodeSize(dockId, defaultSize);
    ImGui::DockBuilderFinish(dockId);

    _dockId = dockId;
    _pendingDockId = dockId;
    _pendingAnimationDockId = dockId;
    _pendingOpenAnimationView = true;
    _focusPrefabFrames = 10;
    _applyDefaultWindowLayout = false;
}

void Prefab_View::Open_PendingAnimationView()
{
    _pendingOpenAnimationView = false;

    Shared<ModelCom> modelCom = Find_FirstModelComponent();
    if (modelCom == nullptr)
        return;

    Shared<Editor_Window> animationWindow = EDITOR->Get_Window(L"Animation");
    Shared<Animation_View> animationView = dynamic_pointer_cast<Animation_View>(animationWindow);
    if (animationView == nullptr)
        return;

    string preferredAnimation{};
    if (modelCom->Get_NumAnimations() > 0)
    {
        if (const char* animationName = modelCom->Get_AnimationName(0))
            preferredAnimation = animationName;
    }

    animationView->Open_FromModelComponent(modelCom, preferredAnimation, _pendingAnimationDockId, false);
}

void Prefab_View::Draw_Header()
{
    if (_prefabName.empty())
    {
        ImGui::TextDisabled("No prefab opened.");
        return;
    }

    ImGui::Text("Prefab: %s", _prefabName.c_str());
    ImGui::SameLine();

    if (ImGui::Button("Save", ImVec2(90.f, 0.f)))
        Save();

    ImGui::SameLine();

    if (ImGui::Button("Close", ImVec2(90.f, 0.f)))
        Close_Prefab();

    ImGui::SameLine();

    ImGui::Checkbox("Show Prefab Collider", &_showPrefabCollider);

    ImGui::SameLine();

    ImGui::Checkbox("Show Grid", &_showPreviewGrid);

    if (!_sourceStaticClass.empty())
        ImGui::TextDisabled("Static Class: %s", _sourceStaticClass.c_str());

    ImGui::TextDisabled("%s", _prefabPath.string().c_str());
}

void Prefab_View::Draw_StructurePanel()
{
    ImGui::BeginChild("PrefabStructurePanel", ImVec2(0.f, 0.f), false);

    ImGui::TextUnformatted("Structure");
    ImGui::Separator();

    if (_prefabName.empty())
    {
        ImGui::TextDisabled("Open a prefab from Content Browser.");
        ImGui::EndChild();
        return;
    }

    if (_previewObject == nullptr)
    {
        ImGui::TextDisabled("Failed to load preview object.");
        ImGui::EndChild();
        return;
    }

    Draw_GameObjectTree(_previewObject, L"Root");

    ImGui::EndChild();
}


void Prefab_View::Draw_PreviewPanel()
{
    ImGui::BeginChild("PrefabPreviewPanel", ImVec2(0.f, 0.f), false);

    ImGui::TextUnformatted("Preview");
    ImGui::Separator();

    ImVec2 previewSize = ImGui::GetContentRegionAvail();
    if (previewSize.x < 1.f)
        previewSize.x = 1.f;
    if (previewSize.y < 1.f)
        previewSize.y = 1.f;

    _previewWidth = max(1u, static_cast<uint32>(previewSize.x));
    _previewHeight = max(1u, static_cast<uint32>(previewSize.y));

    const ImVec2 previewPos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(
        "PrefabPreviewCanvas",
        previewSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    _isPreviewHovered = ImGui::IsItemHovered();
    _isPreviewActive = ImGui::IsItemActive();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (_previewRenderTarget && _previewRenderTarget->Get_SRV())
    {
        ImTextureID textureId =
            static_cast<ImTextureID>(reinterpret_cast<intptr_t>(_previewRenderTarget->Get_SRV()));

        drawList->AddImage(
            textureId,
            previewPos,
            ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y));
    }
    else
    {
        drawList->AddRectFilled(
            previewPos,
            ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y),
            IM_COL32(34, 38, 44, 255));

        drawList->AddRect(
            previewPos,
            ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y),
            IM_COL32(85, 95, 110, 255));

        drawList->AddText(
            ImVec2(previewPos.x + 12.f, previewPos.y + 12.f),
            IM_COL32(210, 216, 224, 255),
            "Preview RenderTarget is not ready.");
    }

    ImGui::EndChild();
}

void Prefab_View::Draw_InspectorPanel()
{
    ImGui::BeginChild("PrefabInspectorPanel", ImVec2(0.f, 0.f), false);

    ImGui::TextUnformatted("Inspector");
    ImGui::Separator();

    if (_prefabName.empty())
    {
        ImGui::TextDisabled("No prefab opened.");
        ImGui::EndChild();
        return;
    }

    if (_previewObject == nullptr)
    {
        ImGui::TextDisabled("Preview object is not loaded.");
        ImGui::EndChild();
        return;
    }

    Shared<GameObject> selectedObject = Find_GameObjectByKey(_previewObject, _selectedObjectKey);
    if (selectedObject == nullptr)
        selectedObject = _previewObject;

    switch (_selectionType)
    {
    case SelectionType::GameObject:
    {
        const Engine::ClassReflectionInfo& info = selectedObject->Get_ReflectionInfo();

        if (info.properties.empty())
        {
            ImGui::TextDisabled("No GameObject reflection properties.");
        }
        else
        {
            Inspector::Draw_ReflectionSection(info.className, selectedObject.get(), info);
        }
        break;
    }

    case SelectionType::Component:
    {
        Shared<Component> selectedComponent = nullptr;

        selectedObject->Visit_Components([&](const wstring& tag, const Shared<Component>& component)
        {
            const wstring componentKey = _selectedObjectKey + L"::" + tag;
            if (componentKey == _selectedComponentKey)
                selectedComponent = component;
        });

        if (selectedComponent == nullptr)
        {
            ImGui::TextDisabled("Selected component not found.");
            break;
        }

        if (Inspector::Draw_ComponentOverride(selectedComponent))
            break;

        const Engine::ClassReflectionInfo& info = selectedComponent->Get_ReflectionInfo();
        if (info.properties.empty())
        {
            ImGui::TextDisabled("No reflection properties.");
            break;
        }

        Inspector::Draw_ReflectionSection(info.className, selectedComponent.get(), info);
        break;
    }

    default:
        ImGui::TextDisabled("Nothing selected.");
        break;
    }

    ImGui::EndChild();
}


void Prefab_View::Draw_GameObjectTree(const Shared<GameObject>& gameObject, const wstring& objectKey)
{
    CHECK_NULL(gameObject);

    string label = "GameObject";
    const ClassReflectionInfo& info = gameObject->Get_ReflectionInfo();

    if (!info.displayName.empty())
        label = info.displayName;
    else if (!info.className.empty())
        label = info.className;
    else
        label = String::ToString(gameObject->Get_Name());

    const bool isPartObject = dynamic_pointer_cast<PartObject>(gameObject) != nullptr;
    if (isPartObject)
        label = "Part - " + label;

    const bool isSelected =
        (_selectionType == SelectionType::GameObject && _selectedObjectKey == objectKey);

    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
    if (isSelected)
        nodeFlags |= ImGuiTreeNodeFlags_Selected;

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
    const bool hasChildren = container && !container->Get_PartObjects().empty();
    if (!hasChildren)
        nodeFlags |= ImGuiTreeNodeFlags_Leaf;

    if (isPartObject)
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(244, 206, 120, 255));

    const bool isOpen = ImGui::TreeNodeEx(String::ToString(objectKey).c_str(), nodeFlags, "%s", label.c_str());

    if (isPartObject)
        ImGui::PopStyleColor();

    if (ImGui::IsItemClicked())
    {
        _selectionType = SelectionType::GameObject;
        _selectedObjectKey = objectKey;
        _selectedComponentKey.clear();
    }

    if (isOpen)
    {
        Draw_ComponentEntries(gameObject, objectKey);

        if (container)
        {
            if (!container->Get_PartObjects().empty())
                ImGui::SeparatorText("Parts");

            for (const auto& [partTag, partObject] : container->Get_PartObjects())
            {
                if (partObject == nullptr)
                    continue;

                const wstring childKey = objectKey + L"/" + partTag;
                Draw_GameObjectTree(partObject, childKey);
            }
        }

        ImGui::TreePop();
    }
}

void Prefab_View::Draw_ComponentEntries(const Shared<GameObject>& gameObject, const wstring& objectKey)
{
    CHECK_NULL(gameObject);

    bool hasComponents = false;
    gameObject->Visit_Components([&](const wstring&, const Shared<Component>& component)
    {
        if (component != nullptr)
            hasComponents = true;
    });

    if (hasComponents)
        ImGui::SeparatorText("Components");

    gameObject->Visit_Components([&](const wstring& tag, const Shared<Component>& component)
    {
        CHECK_NULL(component);

        const wstring componentKey = objectKey + L"::" + tag;

        const bool isSelected =
            (_selectionType == SelectionType::Component && _selectedComponentKey == componentKey);

        string label = String::ToString(tag);
        const ClassReflectionInfo& info = component->Get_ReflectionInfo();

        if (!info.displayName.empty())
            label = info.displayName + " (" + label + ")";
        else if (!info.className.empty())
            label = info.className + " (" + label + ")";

        ImGui::Indent();

        if (ImGui::Selectable(label.c_str(), isSelected))
        {
            _selectionType = SelectionType::Component;
            _selectedObjectKey = objectKey;
            _selectedComponentKey = componentKey;
        }

        ImGui::Unindent();
    });
}

Shared<GameObject> Prefab_View::Find_GameObjectByKey(const Shared<GameObject>& rootObject, const wstring& objectKey) const
{
    if (rootObject == nullptr)
        return nullptr;

    if (objectKey.empty() || objectKey == L"Root")
        return rootObject;

    function<Shared<GameObject>(const Shared<GameObject>&, const wstring&)> findRecursive =
        [&](const Shared<GameObject>& currentObject, const wstring& currentKey) -> Shared<GameObject>
    {
        if (currentObject == nullptr)
            return nullptr;

        if (currentKey == objectKey)
            return currentObject;

        Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(currentObject);
        if (container == nullptr)
            return nullptr;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            if (partObject == nullptr)
                continue;

            const wstring childKey = currentKey + L"/" + partTag;
            Shared<GameObject> foundObject = findRecursive(partObject, childKey);
            if (foundObject != nullptr)
                return foundObject;
        }

        return nullptr;
    };

    return findRecursive(rootObject, L"Root");
}

Editor_Camera_Free::EDITOR_CAMERA_DESC Prefab_View::Build_PreviewCameraDesc(const Shared<GameObject>& gameObject)
{
    UNREFERENCED_PARAMETER(gameObject);

    Editor_Camera_Free::EDITOR_CAMERA_DESC desc{};
    desc.eye = XMFLOAT4(1.15f, 1.35f, 5.2f, 1.f);
    desc.at = XMFLOAT4(0.25f, 1.0f, 0.f, 1.f);
    desc.fovy = XM_PIDIV4;
    desc.nearPlane = 0.1f;
    desc.farPlane = 100.f;
    desc.yaw = 0.f;
    desc.pitch = 0.f;
    desc.mouseSensor = 0.15f;
    desc.speedPerSec = 5.f;
    desc.degreePerSec = 180.f;

    return desc;
}

void Prefab_View::Apply_PreviewCameraTransform(const Shared<Editor_Camera_Free>& camera)
{
    if (camera == nullptr)
        return;

    Shared<TransformCom> cameraTransform = camera->Get_Transform();
    if (cameraTransform == nullptr)
        return;

    cameraTransform->Set_WorldPosition(Vec3(-1.75f, 2.35f, 6.5f));
    cameraTransform->LookAt(Vec3(-0.35f, -3.65f, -14.8f));
}

void Prefab_View::Disable_PreviewAnimNotifyExecution(const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr)
        return;

    gameObject->Visit_Components([&](const wstring&, const Shared<Component>& component)
    {
        Shared<ModelCom> modelCom = dynamic_pointer_cast<ModelCom>(component);
        if (modelCom != nullptr)
            modelCom->Set_AnimNotifyExecutionEnabled(false);
    });

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(gameObject);
    if (container == nullptr)
        return;

    for (const auto& [partTag, partObject] : container->Get_PartObjects())
    {
        UNREFERENCED_PARAMETER(partTag);
        Disable_PreviewAnimNotifyExecution(partObject);
    }
}

bool Prefab_View::Is_AnimationViewFocused()
{
    Shared<Editor_Window> animationWindow = EDITOR->Get_Window(L"Animation");
    Shared<Animation_View> animationView = dynamic_pointer_cast<Animation_View>(animationWindow);
    return animationView != nullptr && animationView->Is_Open() && animationView->Is_Focused();
}

void Prefab_View::Ensure_PreviewRenderTarget(uint32 width, uint32 height)
{
    width = max(1u, width);
    height = max(1u, height);

    // 처음 열었을 때
    if (_previewRenderTarget == nullptr)
    {
        _previewRenderTarget = RenderTarget::Create(
            GAME->Get_Device(),
            GAME->Get_Context(),
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            Vec4(0.15f, 0.15f, 0.18f, 1.f),
            true);
        return;
    }

     _previewRenderTarget->Resize(
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        Vec4(0.15f, 0.15f, 0.18f, 1.f),
        true);
}

void Prefab_View::Ensure_PreviewGridResources()
{
    if (_previewGridBatch && _previewGridEffect && _previewGridInputLayout && _previewGridDepthState)
        return;

    static constexpr D3D11_INPUT_ELEMENT_DESC kGridInputElements[] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",       0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    _previewGridBatch = make_shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>(GAME->Get_Context().Get());
    _previewGridEffect = make_shared<DirectX::BasicEffect>(GAME->Get_Device().Get());

    if (_previewGridBatch == nullptr || _previewGridEffect == nullptr)
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        return;
    }

    _previewGridEffect->SetVertexColorEnabled(true);

    const void* shaderByteCode = nullptr;
    size_t shaderByteCodeLength = 0;
    _previewGridEffect->GetVertexShaderBytecode(&shaderByteCode, &shaderByteCodeLength);

    if (FAILED(GAME->Get_Device()->CreateInputLayout(
        kGridInputElements,
        static_cast<UINT>(_countof(kGridInputElements)),
        shaderByteCode,
        shaderByteCodeLength,
        _previewGridInputLayout.GetAddressOf())))
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        _previewGridInputLayout.Reset();
        return;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;

    if (FAILED(GAME->Get_Device()->CreateDepthStencilState(&depthDesc, _previewGridDepthState.GetAddressOf())))
    {
        _previewGridBatch.reset();
        _previewGridEffect.reset();
        _previewGridInputLayout.Reset();
        _previewGridDepthState.Reset();
    }
}

void Prefab_View::Reset_PreviewCameraPose()
{
    if (_previewObject == nullptr || _previewCamera == nullptr)
        return;

    _previewCamera->Apply_EditorDesc(Build_PreviewCameraDesc(_previewObject));
    Apply_PreviewCameraTransform(_previewCamera);
}

void Prefab_View::Lock_PreviewObjectToOrigin()
{
    if (_previewObject == nullptr)
        return;

    Shared<TransformCom> previewTransform = _previewObject->Get_Transform();
    if (previewTransform == nullptr)
        return;

    previewTransform->Set_WorldPosition(Vec3::Zero);
}

void Prefab_View::Draw_PreviewGrid() const
{
    if (!_showPreviewGrid)
        return;

    if (!_previewGridBatch || !_previewGridEffect || !_previewGridInputLayout || !_previewGridDepthState)
        return;

    ComPtr<Context> context = GAME->Get_Context();
    if (context == nullptr)
        return;

    _previewGridEffect->SetWorld(Matrix::Identity);
    _previewGridEffect->SetView(*GAME->Get_Transform(D3DTS::View));
    _previewGridEffect->SetProjection(*GAME->Get_Transform(D3DTS::Proj));

    // GS particle/effect pass가 남긴 상태를 정리해야 한다.
    ID3D11GeometryShader* nullGeometryShader = nullptr;
    context->GSSetShader(nullGeometryShader, nullptr, 0);

    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRVs);

    context->RSSetState(nullptr);
    context->IASetInputLayout(_previewGridInputLayout.Get());
    context->OMSetDepthStencilState(_previewGridDepthState.Get(), 0);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    _previewGridEffect->Apply(context.Get());

    _previewGridBatch->Begin();

    const Vec3 gridCenter = Vec3::Zero;

    constexpr int gridDivs = 40;
    constexpr float gridHalfExtent = 20.f;
    const float step = (gridHalfExtent * 2.f) / static_cast<float>(gridDivs);
    const float y = 0.f;

    const XMVECTOR gridColor = XMVectorSet(0.58f, 0.58f, 0.62f, 1.f);
    const XMVECTOR xAxisColor = XMVectorSet(0.95f, 0.32f, 0.32f, 1.f);
    const XMVECTOR zAxisColor = XMVectorSet(0.28f, 0.72f, 1.f, 1.f);

    for (int i = 0; i <= gridDivs; ++i)
    {
        const float offset = -gridHalfExtent + step * static_cast<float>(i);

        const XMVECTOR colorX = (fabsf(offset) < 0.0001f) ? zAxisColor : gridColor;
        const XMVECTOR colorZ = (fabsf(offset) < 0.0001f) ? xAxisColor : gridColor;

        DirectX::VertexPositionColor lineXStart(
            Vec3(gridCenter.x - gridHalfExtent, y, gridCenter.z + offset),
            colorX);
        DirectX::VertexPositionColor lineXEnd(
            Vec3(gridCenter.x + gridHalfExtent, y, gridCenter.z + offset),
            colorX);
        _previewGridBatch->DrawLine(lineXStart, lineXEnd);

        DirectX::VertexPositionColor lineZStart(
            Vec3(gridCenter.x + offset, y, gridCenter.z - gridHalfExtent),
            colorZ);
        DirectX::VertexPositionColor lineZEnd(
            Vec3(gridCenter.x + offset, y, gridCenter.z + gridHalfExtent),
            colorZ);
        _previewGridBatch->DrawLine(lineZStart, lineZEnd);
    }

    _previewGridBatch->End();
}

Shared<Prefab_View> Prefab_View::Create()
{
    return make_shared<Prefab_View>();
}
