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
                _ctx->_modelOverrides[filename] = -1;
                _ctx->_dirtyPassOverrides = true;
            }
        }
    }

    ImGui::Separator();

    // 콤보용 이름 배열
    const char* names[g_meshPassOptionCount];
    for (int32 i = 0; i < g_meshPassOptionCount; ++i)
    {
        names[i] = g_meshPassOptions[i].first;
    }

    vector<string> toRemove;
    for (auto& [filename, passIndex] : _ctx->_modelOverrides)
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

        int32 current = 0;
        for (int32 i = 0; i < g_meshPassOptionCount; ++i)
        {
            if ((int32)g_meshPassOptions[i].second == passIndex)
            {
                current = i;
                break;
            }
        }

        if (ImGui::Combo("Pass", &current, names, g_meshPassOptionCount))
        {
            passIndex = (int32)g_meshPassOptions[current].second;
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
