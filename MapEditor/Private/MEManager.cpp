#include "pch.h"
#include "MEManager.h"
#include "PanelViewport.h"
#include "PanelHierarchy.h"
#include "PanelInspector.h"
#include "PanelAssetBrowser.h"
#include "PanelSnap.h"
#include "SceneContext.h"
#include "MECamera.h"
#include "PanelModelOverride.h"
#include "PanelLandscape.h"

extern HWND g_hWnd;

namespace
{
    fs::path ResolveMapResourceRoot()
    {
        fs::path repoRoot = fs::path(__FILE__).parent_path().parent_path().parent_path();
        fs::path mapRoot = repoRoot / "Client" / "Bin" / "Resources" / "Map";

        error_code ec;
        fs::path resolved = fs::weakly_canonical(mapRoot, ec);
        return (!ec && !resolved.empty()) ? resolved : mapRoot;
    }

    bool IsSplitMapInstanceFolder(const fs::path& path)
    {
        return fs::exists(path / "StaticMesh") ||
            fs::exists(path / "InstanceMesh") ||
            fs::exists(path / "PostProcessVolume") ||
            fs::exists(path / "Water") ||
            fs::exists(path / "ETC");
    }

    fs::path ResolveMapInstanceRootForMenu()
    {
        return ResolveMapResourceRoot() / "MapInstance";
    }
}

MEManager::~MEManager()
{

}

HRESULT MEManager::Initialize(Shared<PanelViewport> viewport, SceneContext* ctx, MECamera* camera, const LandscapePanelContext& landscapeCtx)
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
    Add(PanelModelOverride::Create(ctx, viewport));
    Add(PanelLandscape::Create(ctx, landscapeCtx));

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
    static int skipFrames = 0;
    if (skipFrames > 0)
    {
        skipFrames--;
        ImGui::GetIO().AppFocusLost = true; // 대화상자 복귀 프레임 동안 입력 꼬임 차단
    }
    else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S) && !ImGui::GetIO().WantTextInput)
    {
        skipFrames = 3; // 다이얼로그 복귀 후 키 릴리즈 메시지가 처리될 때까지 3프레임간 입력을 스킵
        SaveMapDialog();
    }

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
                _ctx->ClearScene();
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
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameA(&ofn))
                {
                    _ctx->LoadFromFile(buf);
                }
            }

            if (ImGui::MenuItem("Append Map..."))
            {
                char buf[MAX_PATH] = {};
                OPENFILENAMEA ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
                ofn.lpstrFile = buf;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrInitialDir = _mapSavePath;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameA(&ofn))
                {
                    _ctx->LoadFromFile(buf, true);
                }
            }

            if (ImGui::MenuItem("Save Map", "Ctrl+S"))
            {
                SaveMapDialog();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Clear Map"))
            {
                _confirmClearOpen = true;
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

        if (ImGui::BeginMenu("Camera"))
        {
            if (ImGui::MenuItem("Save Cam Data"))
            {
                string camFile = string(_camDataPath) + "cam_bookmarks.json";
                _ctx->SaveCamData(camFile);
            }
            if (ImGui::MenuItem("Load Cam Data"))
            {
                string camFile = string(_camDataPath) + "cam_bookmarks.json";
                _ctx->LoadCamData(camFile);
            }

            ImGui::Separator();

            for (int i = 0; i < CameraBookmarkCount; ++i)
            {
                ImGui::PushID(i);
                auto& bm = _ctx->_cameraBookmarks[i];

                if (ImGui::SmallButton("Save"))
                {
                    bm.position = _camera->GetPosition();
                    bm.yaw = _camera->GetYaw();
                    bm.pitch = _camera->GetPitch();
                    bm.valid = true;
                }
                ImGui::SameLine();
                if (bm.valid)
                {
                    if (ImGui::SmallButton("Go"))
                    {
                        _camera->ApplyBookmark(bm.position, bm.yaw, bm.pitch);
                    }
                    ImGui::SameLine();
                    ImGui::Text("[%d] %.1f  %.1f  %.1f", i + 1, bm.position.x, bm.position.y, bm.position.z);
                }
                else
                {
                    ImGui::TextDisabled("[%d] empty", i + 1);
                }
                ImGui::PopID();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Map Explicit Tools"))
        {
            if (ImGui::MenuItem("Export Split Instance"))
            {
                string defaultName = fs::path(_ctx->_currentFilePath).stem().string();
                if (defaultName.empty() || defaultName == "Untitled")
                {
                    defaultName = String::ToString(_ctx->_mapName);
                }
                strncpy_s(_exportNameBuf, defaultName.c_str(), sizeof(_exportNameBuf) - 1);
                _exportPopupOpen = true;
            }
            if (ImGui::BeginMenu("Import Split Instance"))
            {
                fs::path instanceRoot = ResolveMapInstanceRootForMenu();
                if (fs::exists(instanceRoot))
                {
                    for (const auto& entry : fs::directory_iterator(instanceRoot))
                    {
                        if (entry.is_directory() &&
                            entry.path().filename().string() != "MapInstance" &&
                            IsSplitMapInstanceFolder(entry.path()))
                        {
                            string mapName = entry.path().filename().string();
                            if (ImGui::MenuItem(mapName.c_str()))
                            {
                                _ctx->Import_MapInstance(mapName);
                            }
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if (_exportPopupOpen)
    {
        ImGui::OpenPopup("Export Split Instance");
        _exportPopupOpen = false;
    }

    if (ImGui::BeginPopupModal("Export Split Instance", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Export 폴더 이름을 입력하세요.");
        ImGui::InputText("##exportName", _exportNameBuf, sizeof(_exportNameBuf));

        fs::path exportPath = ResolveMapInstanceRootForMenu() / _exportNameBuf;
        if (fs::exists(exportPath))
        {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "경고: 해당 폴더가 이미 존재합니다. 덮어씌워집니다.");
        }

        ImGui::Separator();
        if (ImGui::Button("Export", ImVec2(120, 0)))
        {
            if (strlen(_exportNameBuf) > 0)
            {
                _ctx->Export_MapInstance(_exportNameBuf);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("취소", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (_confirmClearOpen)
    {
        ImGui::OpenPopup("맵 전체 삭제");
    }

    if (ImGui::BeginPopupModal("맵 전체 삭제", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("모든 오브젝트를 삭제합니다. 되돌릴 수 없습니다.");
        ImGui::Separator();
        if (ImGui::Button("삭제", ImVec2(120, 0)))
        {
            _ctx->ClearScene();
            _confirmClearOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("취소", ImVec2(120, 0)))
        {
            _confirmClearOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
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

Unique<MEManager> MEManager::Create(Shared<PanelViewport> viewport, SceneContext* ctx, MECamera* camera, const LandscapePanelContext& landscapeCtx)
{
    auto instance = make_unique<MEManager>();

    CHECK_FAILED(instance->Initialize(viewport, ctx, camera, landscapeCtx), nullptr);

    return instance;
}

void MEManager::SaveMapDialog()
{
    ImGui::GetIO().AppFocusLost = true; // 다이얼로그를 띄우기 전 마우스/키 상태 강제 릴리즈

    char buf[MAX_PATH] = {};
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd; // 부모 윈도우 연결하여 모달 포커스 보장
    ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = _mapSavePath;
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn))
    {
        _ctx->SaveToFile(buf);
    }
}
