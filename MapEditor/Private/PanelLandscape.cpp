#include "PanelLandscape.h"
#include "SceneContext.h"

PanelLandscape::PanelLandscape()
    : MEWindow{ L"Landscape" }
{
}

HRESULT PanelLandscape::Initialize(SceneContext* ctx, const LandscapePanelContext& landscapeCtx)
{
    _ctx = ctx;
    _sculptState = landscapeCtx.sculptState;
    _onAdd = landscapeCtx.onAdd;
    _onRemove = landscapeCtx.onRemove;
    _onSave = landscapeCtx.onSave;
    _onLoad = landscapeCtx.onLoad;
    _onApplyNoise = landscapeCtx.onApplyNoise;
    _onApplyErosion = landscapeCtx.onApplyErosion;
    _onAddLayer = landscapeCtx.onAddLayer;
    _onRemoveLayer = landscapeCtx.onRemoveLayer;
    _onSetBaseLayer = landscapeCtx.onSetBaseLayer;
    _onModify = landscapeCtx.onModify;

    return S_OK;
}

void PanelLandscape::Update(float timeDelta)
{
}

void PanelLandscape::Render()
{
    bool open = IsOpen();
    if (ImGui::Begin("Landscape", &open))
    {
        Render_LandscapeList();
        ImGui::Separator();

        Render_SculptTools();
        ImGui::Separator();

        Render_PaintTools();
        ImGui::Separator();

        Render_WorldOffset();
        ImGui::Separator();

        Render_SaveLoad();
        ImGui::Separator();

        Render_Noise();
        ImGui::Separator();

        Render_Erosion();

        if (_showAddLayerDialog)
        {
            Render_AddLayerDialog();
        }
    }

    ImGui::End();
    SetOpen(open);

    if (_showCreateDialog)
    {
        Render_CreateDialog();
    }
}

void PanelLandscape::Render_LandscapeList()
{
    ImGui::Text("Landscapes");

    const int32 count = static_cast<int32>(_ctx->_landscapes.size());

    if (count == 0)
    {
        ImGui::TextDisabled("None");
    }
    else
    {
        for (int32 i = 0; i < count; ++i)
        {
            ImGui::PushID(i);

            const string label = "Landscape_" + to_string(i);
            bool selected = (_ctx->_activeLandscapeIdx == i);
            if (ImGui::Selectable(label.c_str(), selected))
            {
                _ctx->_activeLandscapeIdx = i;
            }

            ImGui::PopID();
        }
    }

    ImGui::Spacing();

    if (ImGui::Button("+ 추가"))
    {
        _showCreateDialog = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("- 삭제"))
    {
        if (_ctx->_activeLandscapeIdx >= 0 && _onRemove)
        {
            _onRemove(_ctx->_activeLandscapeIdx);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("수정"))
    {
        int32 idx = _ctx->_activeLandscapeIdx;
        if (idx >= 0 && idx < static_cast<int32>(_ctx->_landscapes.size()))
        {
            auto& actor = _ctx->_landscapes[idx];
            auto vib = actor->Get_VIBuffer();
            Vec3 pos = actor->Get_Transform()->Get_Position();
            _createCountX = static_cast<int32>(vib->Get_CountX());
            _createCountZ = static_cast<int32>(vib->Get_CountZ());
            _createSizeX = vib->Get_SizeX();
            _createSizeZ = vib->Get_SizeZ();
            _createChunkSize = static_cast<int32>(vib->Get_ChunkSize());
            _createOffsetX = pos.x;
            _createOffsetY = pos.y;
            _createOffsetZ = pos.z;
            _modifyTargetIdx = idx;
            _isModifyMode = true;
            _showCreateDialog = true;
        }
    }
}

void PanelLandscape::Render_CreateDialog()
{
    ImGui::SetNextWindowSize(ImVec2(320.f, 300.f), ImGuiCond_Always);
    if (ImGui::Begin("Landscape 생성", &_showCreateDialog, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::InputText("이름", _createName, sizeof(_createName));
        ImGui::InputInt("쿼드 X", &_createCountX);
        ImGui::InputInt("쿼드 Z", &_createCountZ);
        ImGui::DragFloat("크기 X", &_createSizeX, 0.1f, 0.1f, 100.f, "%.2f");
        ImGui::DragFloat("크기 Z", &_createSizeZ, 0.1f, 0.1f, 100.f, "%.2f");
        ImGui::Separator();
        ImGui::DragFloat("오프셋 X", &_createOffsetX, 1.f, -10000.f, 10000.f, "%.1f");
        ImGui::DragFloat("오프셋 Y", &_createOffsetY, 1.f, -10000.f, 10000.f, "%.1f");
        ImGui::DragFloat("오프셋 Z", &_createOffsetZ, 1.f, -10000.f, 10000.f, "%.1f");
        ImGui::Separator();
        ImGui::DragInt("청크 크기", &_createChunkSize, 1, 4, 64);
        ImGui::TextDisabled("※ 4의 배수 권장 (4, 8, 16, 32, 64)");
        ImGui::Separator();

        _createCountX = max(2, _createCountX);
        _createCountZ = max(2, _createCountZ);

        const float totalX = _createCountX * _createSizeX;
        const float totalZ = _createCountZ * _createSizeZ;
        ImGui::TextDisabled("총 크기: %.1f x %.1f", totalX, totalZ);
        ImGui::TextDisabled("정점 수: %d", (_createCountX + 1) * (_createCountZ + 1));

        ImGui::Spacing();
        if (_isModifyMode)
        {
            if (ImGui::Button("수정", ImVec2(120.f, 0.f)))
            {
                LandscapeConfig cfg{};
                cfg.name = _createName;
                cfg.countX = static_cast<uint32>(_createCountX);
                cfg.countZ = static_cast<uint32>(_createCountZ);
                cfg.sizeX = _createSizeX;
                cfg.sizeZ = _createSizeZ;
                cfg.worldOffsetX = _createOffsetX;
                cfg.worldOffsetY = _createOffsetY;
                cfg.worldOffsetZ = _createOffsetZ;
                cfg.chunkSize = static_cast<uint32>(max(4, _createChunkSize));

                if (_onModify)
                {
                    _onModify(_modifyTargetIdx, cfg);
                }
                _showCreateDialog = false;
                _isModifyMode = false;
            }
        }
        else
        {
            if (ImGui::Button("생성", ImVec2(120.f, 0.f)))
            {
                LandscapeConfig cfg{};
                cfg.name = _createName;
                cfg.countX = static_cast<uint32>(_createCountX);
                cfg.countZ = static_cast<uint32>(_createCountZ);
                cfg.sizeX = _createSizeX;
                cfg.sizeZ = _createSizeZ;
                cfg.worldOffsetX = _createOffsetX;
                cfg.worldOffsetY = _createOffsetY;
                cfg.worldOffsetZ = _createOffsetZ;
                cfg.chunkSize = static_cast<uint32>(max(4, _createChunkSize));

                if (_onAdd)
                {
                    _onAdd(cfg);
                }
                _showCreateDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("취소", ImVec2(120.f, 0.f)))
        {
            _showCreateDialog = false;
            _isModifyMode = false;
        }
    }
    ImGui::End();
}

void PanelLandscape::Render_SculptTools()
{
    ImGui::Text("Sculpt");

    const auto Btn = [&](const char* label, ESculptMode mode)
        {
            bool active = (_sculptState->mode == mode);
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }

            if (ImGui::Button(label, ImVec2(70.f, 0.f)))
            {
                _sculptState->mode = active ? ESculptMode::None : mode;
            }

            if (active)
            {
                ImGui::PopStyleColor();
            }
        };

    Btn("Raise", ESculptMode::Raise);
    ImGui::SameLine();

    Btn("Lower", ESculptMode::Lower);
    ImGui::SameLine();

    Btn("Smooth", ESculptMode::Smooth);
    ImGui::SameLine();

    Btn("Flatten", ESculptMode::Flatten);
    ImGui::SameLine();

    {
        bool active = (_sculptState->mode == ESculptMode::None);
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button("Off", ImVec2(40.f, 0.f)))
        {
            _sculptState->mode = ESculptMode::None;
        }
        if (active)
        {
            ImGui::PopStyleColor();
        }
    }

    ImGui::Spacing();
    ImGui::DragFloat("반경", &_sculptState->brushRadius, 0.1f, 0.5f, 200.f, "%.1f");
    ImGui::DragFloat("강도", &_sculptState->brushStrength, 0.01f, 0.01f, 5.f, "%.2f");

    if (_sculptState->mode == ESculptMode::Flatten)
    {
        ImGui::DragFloat("목표 높이", &_sculptState->flattenHeight, 0.1f, -1000.f, 1000.f, "%.1f");
    }
}

void PanelLandscape::Render_WorldOffset()
{
    if (_ctx->_activeLandscapeIdx < 0 || _ctx->_activeLandscapeIdx >= static_cast<int32>(_ctx->_landscapes.size()))
    {
        return;
    }

    auto& actor = (_ctx->_landscapes)[_ctx->_activeLandscapeIdx];
    Vec3 pos = actor->Get_Transform()->Get_Position();

    ImGui::Text("World Offset");
    if (ImGui::DragFloat3("위치##landscape", reinterpret_cast<float*>(&pos), 1.f, -10000.f, 10000.f, "%.1f"))
    {
        actor->Get_Transform()->Set_Position(pos);
    }
}

void PanelLandscape::Render_SaveLoad()
{
    ImGui::Text("Heightmap");

    if (ImGui::Button("Save"))
    {
        if (_ctx->_activeLandscapeIdx >= 0 && _onSave)
        {
            WCHAR buf[MAX_PATH] = {};
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile = buf;
            ofn.lpstrFilter = L"Heightmap Files\0*.hmap\0All Files\0*.*\0";
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrDefExt = L"hmap";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            if (GetSaveFileNameW(&ofn))
            {
                _onSave(_ctx->_activeLandscapeIdx, buf);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        if (_onLoad)
        {
            WCHAR buf[MAX_PATH] = {};
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = L"Heightmap Files\0*.hmap\0All Files\0*.*\0";
            ofn.lpstrFile = buf;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameW(&ofn))
            {
                _onLoad(-1, buf);
            }
        }
    }
}

void PanelLandscape::Render_Noise()
{
    ImGui::Text("Noise");

    ImGui::DragFloat("스케일", &_noiseParams.scale, 0.001f, 0.001f, 1.f, "%.3f");
    ImGui::DragFloat("진폭", &_noiseParams.amplitude, 0.5f, 0.1f, 500.f, "%.1f");
    ImGui::DragInt("옥타브", &_noiseParams.octaves, 1, 1, 8);
    ImGui::DragFloat("Persistence", &_noiseParams.persistence, 0.01f, 0.1f, 1.f, "%.2f");
    ImGui::DragInt("Seed", &_noiseParams.seed, 1, 0, 99999);
    ImGui::Checkbox("기존에 더하기", &_noiseParams.additive);

    ImGui::Spacing();
    if (ImGui::Button("Noise 적용", ImVec2(120.f, 0.f)))
    {
        if (_ctx->_activeLandscapeIdx >= 0 && _onApplyNoise)
        {
            _onApplyNoise(_ctx->_activeLandscapeIdx, _noiseParams);
        }
    }
}

void PanelLandscape::Render_Erosion()
{
    ImGui::Text("Erosion");

    ImGui::DragInt("반복 횟수", &_erosionParams.iterations, 1, 1, 500);
    ImGui::DragFloat("임계 경사도", &_erosionParams.talusAngle, 0.01f, 0.01f, 10.f, "%.2f");
    ImGui::DragFloat("침식량", &_erosionParams.erosionRate, 0.01f, 0.01f, 1.f, "%.2f");

    ImGui::Spacing();
    if (ImGui::Button("Erosion 적용", ImVec2(120.f, 0.f)))
    {
        if (_ctx->_activeLandscapeIdx >= 0 && _onApplyErosion)
        {
            _onApplyErosion(_ctx->_activeLandscapeIdx, _erosionParams);
        }
    }
}

void PanelLandscape::Render_PaintTools()
{
    ImGui::Text("Paint");

    if (_ctx->_activeLandscapeIdx < 0 || _ctx->_activeLandscapeIdx >= static_cast<int32>(_ctx->_landscapes.size()))
    {
        ImGui::TextDisabled("선택된 지형 없음");
        return;
    }

    auto& actor = _ctx->_landscapes[_ctx->_activeLandscapeIdx];
    const uint32 layerCount = actor->Get_LayerCount();

    bool isPaint = (_sculptState->mode == ESculptMode::Paint);
    if (isPaint)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button("Paint", ImVec2(70.f, 0.f)))
    {
        _sculptState->mode = isPaint ? ESculptMode::None : ESculptMode::Paint;
    }
    if (isPaint)
    {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (ImGui::Button("Off##paint", ImVec2(40.f, 0.f)))
    {
        _sculptState->mode = ESculptMode::None;
    }

    ImGui::Spacing();
    ImGui::Text("레이어 (%u / %u)", layerCount, MAX_LAYERS);
    for (uint32 i = 0; i < layerCount; ++i)
    {
        ImGui::PushID(static_cast<int32>(i));
        int32 sel = static_cast<int32>(_sculptState->paintLayerIdx);
        string label = "Layer " + to_string(i);

        if (ImGui::RadioButton(label.c_str(), &sel, static_cast<int32>(i)))
        {
            _sculptState->paintLayerIdx = static_cast<uint32>(sel);
        }
        ImGui::SameLine();

        string layerFile = fs::path(actor->Get_LayerPath(i)).filename().string();
        ImGui::TextDisabled("(%s)", layerFile.c_str());
        if (i > 0)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
            {
                if (_onRemoveLayer)
                {
                    if (_sculptState->paintLayerIdx >= i)
                    {
                        _sculptState->paintLayerIdx = i - 1;
                    }
                    _onRemoveLayer(_ctx->_activeLandscapeIdx, i);
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("!"))
            {
                if (_onSetBaseLayer)
                {
                    _sculptState->paintLayerIdx = 0;
                    _onSetBaseLayer(_ctx->_activeLandscapeIdx, i);
                }
            }
        }

        string tilingLabel = "Tiling##" + to_string(i);
        string tintLabel = "Tint##" + to_string(i);
        string normalLabel = "Normal Intensity##" + to_string(i);
        string roughLabel = "Roughness x##" + to_string(i);
        string metallicLabel = "Metallic##" + to_string(i);

        float tiling = actor->Get_LayerTiling(i);
        if (ImGui::DragFloat(tilingLabel.c_str(), &tiling, 0.01f, 0.1f, 50.f, "%.2f"))
        {
            actor->Set_LayerTiling(i, tiling);
        }

        Vec4 tint = actor->Get_LayerGroundTint(i);
        if (ImGui::ColorEdit3(tintLabel.c_str(), reinterpret_cast<float*>(&tint)))
        {
            actor->Set_LayerGroundTint(i, tint);
        }

        Vec4 ctrl = actor->Get_LayerControl(i);
        if (ImGui::DragFloat(normalLabel.c_str(), &ctrl.x, 0.01f, 0.f, 10.f, "%.2f"))
        {
            actor->Set_LayerControl(i, ctrl);
        }

        if (ImGui::DragFloat(roughLabel.c_str(), &ctrl.y, 0.01f, 0.f, 10.f, "%.2f"))
        {
            actor->Set_LayerControl(i, ctrl);
        }

        if (ImGui::DragFloat(metallicLabel.c_str(), &ctrl.w, 0.01f, 0.f, 1.f, "%.2f"))
        {
            actor->Set_LayerControl(i, ctrl);
        }

        Vec4 albedo = actor->Get_AlbedoControl(i);
        string satLabel= "Saturation##" + to_string(i);
        string briLabel = "Brightness##" + to_string(i);
        string conLabel = "Contrast##" + to_string(i);
        if (ImGui::DragFloat(satLabel.c_str(), &albedo.x, 0.01f, 0.f, 5.f, "%.3f"))
        {
            actor->Set_AlbedoControl(i, albedo);
        }

        if (ImGui::DragFloat(briLabel.c_str(), &albedo.y, 0.01f, 0.f, 20.f, "%.3f"))
        {
            actor->Set_AlbedoControl(i, albedo);
        }

        if (ImGui::DragFloat(conLabel.c_str(), &albedo.z, 0.01f, 0.f, 5.f, "%.3f"))
        {
            actor->Set_AlbedoControl(i, albedo);
        }

        ImGui::PopID();
    }


    if (layerCount < MAX_LAYERS)
    {
        ImGui::Spacing();
        if (ImGui::Button("+ 레이어 추가"))
        {
            _showAddLayerDialog = true;
            _pendingAlbedo.clear();
            _pendingNormal.clear();
            _pendingRoughness.clear();
        }
    }
}

void PanelLandscape::Render_AddLayerDialog()
{
    ImGui::SetNextWindowSize(ImVec2(420.f, 200.f), ImGuiCond_Always);
    if (ImGui::Begin("레이어 추가", &_showAddLayerDialog, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        auto PickFile = [](const char* label, wstring& path)
            {
                string preview = path.empty() ? "-" : String::ToString(path);
                ImGui::Text("%s: %s", label, preview.c_str());
                ImGui::SameLine();

                string btnLabel = string("선택##") + label;
                if (ImGui::SmallButton(btnLabel.c_str()))
                {
                    WCHAR buf[MAX_PATH] = {};
                    OPENFILENAME ofn{};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.lpstrFilter = L"Texture Files\0*.dds;*.png\0All Files\0*.*\0";
                    ofn.lpstrFile = buf;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                    if (GetOpenFileNameW(&ofn))
                    {
                        path = buf;
                    }
                }
            };

        PickFile("Albedo   ", _pendingAlbedo);
        PickFile("Normal   ", _pendingNormal);
        PickFile("Roughness", _pendingRoughness);

        ImGui::Spacing();
        ImGui::BeginDisabled(_pendingAlbedo.empty());
        if (ImGui::Button("추가", ImVec2(100.f, 0.f)))
        {
            if (_onAddLayer)
            {
                _onAddLayer(_ctx->_activeLandscapeIdx,
                    _pendingAlbedo.c_str(), _pendingNormal.c_str(), _pendingRoughness.c_str());
            }
            _showAddLayerDialog = false;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("취소", ImVec2(100.f, 0.f)))
        {
            _showAddLayerDialog = false;
        }
    }
    ImGui::End();
}

Shared<PanelLandscape> PanelLandscape::Create(SceneContext* ctx, const LandscapePanelContext& landscapeCtx)
{
    auto instance = make_shared<PanelLandscape>();
    if (FAILED(instance->Initialize(ctx, landscapeCtx)))
    {
        return nullptr;
    }

    return instance;
}

