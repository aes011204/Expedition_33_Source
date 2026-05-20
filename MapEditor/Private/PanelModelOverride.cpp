#include "pch.h"
#include "PanelModelOverride.h"
#include "SceneContext.h"

PanelModelOverride::PanelModelOverride(SceneContext* ctx)
    : MEWindow{ L"Model Override" }, _ctx{ ctx }
{
}

void PanelModelOverride::Render()
{
    bool open = IsOpen();
    if (!ImGui::Begin("Model Override", &open))
    {
        ImGui::End();
        SetOpen(open);
        return;
    }

    if (!_ctx)
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("+ 선택 오브젝트 추가"))
    {
        auto& sel = _ctx->_selected;
        if (sel)
        {
            string filename;
            if (!sel->_modelPath.empty())
            {
                filename = String::ToString(fs::path(sel->_modelPath).filename().wstring());
            }
            else if (!sel->_instancedModelPath.empty())
            {
                filename = String::ToString(fs::path(sel->_instancedModelPath).filename().wstring());
            }

            if (!filename.empty() && _ctx->_modelOverrides.find(filename) == _ctx->_modelOverrides.end())
            {
                ModelOverrideEntry newEntry;
                newEntry.shaderType = sel->_modelPath.empty() ? ShaderType::VtxInstanceMeshExact
                    : ShaderType::VtxMesh;
                newEntry.passIndex = -1;
                _ctx->_modelOverrides[filename] = newEntry;
                _ctx->_dirtyPassOverrides = true;
            }
        }
    }

    ImGui::Separator();

    vector<string> toRemove;
    for (auto& [filename, entry] : _ctx->_modelOverrides)
    {
        ImGui::PushID(filename.c_str());

        if (ImGui::Button("X"))
        {
            toRemove.push_back(filename);
            ImGui::PopID();
            continue;
        }
        ImGui::SameLine();

        ImGui::Text("%s", filename.c_str());
        ImGui::SameLine();

        const bool isInstance = (entry.shaderType == ShaderType::VtxInstanceMeshExact);
        auto* opts = isInstance ? g_instanceMeshPassOptions : g_staticMeshPassOptions;
        int32 count = isInstance ? g_instanceMeshPassOptionCount : g_staticMeshPassOptionCount;

        const char* names[16];
        for (int32 i = 0; i < count; ++i)
        {
            names[i] = opts[i].first;
        }

        int32 current = 0;
        for (int32 i = 0; i < count; ++i)
        {
            if ((int32)opts[i].second == entry.passIndex)
            {
                current = i;
                break;
            }
        }

        if (ImGui::Combo("Pass", &current, names, count))
        {
            entry.passIndex = (int32)opts[current].second;
            _ctx->_dirtyPassOverrides = true;
        }

        ImGui::PopID();
    }


    for (auto& f : toRemove)
    {
        _ctx->_modelOverrides.erase(f);
        _ctx->_dirtyPassOverrides = true;
    }

    ImGui::End();
    SetOpen(open);
}

Shared<PanelModelOverride> PanelModelOverride::Create(SceneContext* ctx)
{
    return make_shared<PanelModelOverride>(ctx);
}
