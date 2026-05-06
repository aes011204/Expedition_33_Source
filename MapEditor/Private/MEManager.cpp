#include "pch.h"
#include "MEManager.h"
#include "PanelViewport.h"
#include "PanelHierarchy.h"
#include "PanelInspector.h"
#include "PanelAssetBrowser.h"
#include "PanelSnap.h"
#include "SceneContext.h"
#include "MECamera.h"

MEManager::~MEManager()
{

}

HRESULT MEManager::Initialize(Shared<PanelViewport> viewport, SceneContext* ctx, MECamera* camera)
{
    auto Add = [this](Shared<MEWindow> w) {
        if (w)
        {
            _windows[w->GetName()] = w;
        }};

    _ctx = ctx;
    _camera = camera;

    Add(viewport);
    Add(PanelHierarchy::Create(ctx));
    Add(PanelInspector::Create(ctx, camera));
    Add(PanelAssetBrowser::Create(ctx));

    _snapSettings = make_shared<SnapSettings>();
    viewport->SetSnapSettings(_snapSettings);
    Add(PanelSnap::Create(_snapSettings));

    return S_OK;
}

void MEManager::Update(float timeDelta)
{
    for (auto& [name, window] : _windows)
    {
        if (window)
        {
            window->Update(timeDelta);
        }
    }
}

void MEManager::Render()
{
    RenderDockSpace();

    for (auto& [name, window] : _windows)
    {
        if (window && window->IsOpen())
        {
            window->Render();
        }
    }
}

void MEManager::RenderDockSpace()
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    bool open = true;
    ImGui::Begin("MapEditorDockSpace", &open, flags);
    ImGui::PopStyleVar(2);

    ImGuiID dockId = ImGui::GetID("MapEditorDockSpaceId");
    ImGui::DockSpace(dockId, ImVec2(0.f, 0.f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!layoutInitialized)
    {
        layoutInitialized = true;

        ImGui::DockBuilderRemoveNode(dockId);
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);

        ImGuiID dockLeft, dockRight, dockCenter, dockBottom;

        ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.20f, &dockLeft, &dockCenter);
        ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Right, 0.25f, &dockRight, &dockCenter);
        ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Down, 0.30f, &dockBottom, &dockCenter);

        ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector", dockRight);
        ImGui::DockBuilderDockWindow("Asset Browser", dockBottom);
        ImGui::DockBuilderDockWindow("Viewport", dockCenter);

        ImGui::DockBuilderFinish(dockId);
    }

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Map"))
            {
                _ctx->_objects.clear();
                _ctx->_selected = nullptr;
                _ctx->_mapName = L"Untitled";
                _currentMapPath = "Stage_01.json";
            }

            if (ImGui::MenuItem("Open Map..."))
            {
                char buf[MAX_PATH] = {};
                OPENFILENAMEA ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
                ofn.lpstrFile = buf;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrInitialDir = _mapSavePath;
                ofn.Flags = OFN_FILEMUSTEXIST;
                if (GetOpenFileNameA(&ofn))
                {
                    _ctx->LoadFromFile(buf);
                }
            }

            if (ImGui::MenuItem("Save Map"))
            {
                char buf[MAX_PATH] = {};
                OPENFILENAMEA ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
                ofn.lpstrFile = buf;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrInitialDir = _mapSavePath;
                ofn.lpstrDefExt = "json";
                ofn.Flags = OFN_OVERWRITEPROMPT;
                if (GetSaveFileNameA(&ofn))
                {
                    _ctx->SaveToFile(buf);
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
            {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            for (auto& [name, window] : _windows)
            {
                if (!window)
                {
                    continue;
                }
                bool isOpen = window->IsOpen();
                const string nameStr = String::ToString(name);
                if (ImGui::MenuItem(nameStr.c_str(), nullptr, &isOpen))
                {
                    window->SetOpen(isOpen);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::End();
}

void MEManager::AddWindow(Shared<MEWindow> w)
{
    if (w)
    {
        _windows[w->GetName()] = w;
    }
}

Unique<MEManager> MEManager::Create(Shared<PanelViewport> viewport, SceneContext* ctx, MECamera* camera)
{
    auto instance = make_unique<MEManager>();

    CHECK_FAILED(instance->Initialize(viewport, ctx, camera), nullptr);

    return instance;
}
