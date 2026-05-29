#include "CombatStat_Inspector.h"
#include "CombatStatCom.h"
#include "GameInstance.h"

namespace
{
    string Normalize_RelativePath(const AssetMeta& asset)
    {
        string relativePath = String::ToString(asset.relativePath);
        replace(relativePath.begin(), relativePath.end(), '\\', '/');
        return String::ToLowerCopy(relativePath);
    }

    string Get_AssetFileName(const AssetMeta& asset)
    {
        return String::ToString(fs::path(asset.relativePath).filename().wstring());
    }

    bool StartsWith(const string& value, const string& prefix)
    {
        return value.rfind(prefix, 0) == 0;
    }

    string Normalize_Prefix(string prefix)
    {
        replace(prefix.begin(), prefix.end(), '\\', '/');
        return String::ToLowerCopy(prefix);
    }
}

void CombatStat_Inspector::Draw_CommonRuntimeSection(const Shared<Client::CombatStatCom>& stat)
{
    if (stat == nullptr)
        return;

    Client::CombatStatCom::RuntimeCombatStat& runtime = stat->Get_RuntimeStat();

    float curHp = runtime.CurHp;
    int curShield = static_cast<int>(runtime.CurShield);
    int curAp = static_cast<int>(runtime.CurAp);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Current HP");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat("##CombatCurHp", &curHp, 1.f, 0.f, 999999.f))
    {
        runtime.CurHp = max(0.f, curHp); // 공통 런타임 자원이라 바로 반영
    }

    ImGui::TextDisabled("Current Shield");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragInt("##CombatCurShield", &curShield, 1.f, 0, 999999))
    {
        runtime.CurShield = static_cast<uint32>(max(0, curShield)); // 음수 방지
    }

    ImGui::TextDisabled("Current AP");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragInt("##CombatCurAp", &curAp, 1.f, 0, 999))
    {
        runtime.CurAp = static_cast<uint32>(max(0, curAp)); // 런타임 AP만 직접 수정
    }

    ImGui::TextDisabled("Alive State");
    ImGui::SameLine(120.f);
    ImGui::Text("%d", static_cast<int>(runtime.CurAliveState)); // 지금은 표시만
}

bool CombatStat_Inspector::Draw_WStringPropertyRow(
    const char* label,
    const char* hiddenId,
    wstring& value)
{
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(120.f);

    string valueText = String::ToString(value);
    char buffer[512]{};
    strncpy_s(buffer, valueText.c_str(), _TRUNCATE);

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputText(hiddenId, buffer, IM_ARRAYSIZE(buffer)))
    {
        value = String::ToWString(buffer);
        return true;
    }

    return false;
}

bool CombatStat_Inspector::Draw_PortraitTexturePicker(const char* label, string& portraitGuid,
    const string& requiredRelativePrefix)
{
    bool changed = false;
    const string normalizedPrefix = Normalize_Prefix(requiredRelativePrefix);

    const AssetMeta* selectedAsset = portraitGuid.empty()
        ? nullptr
        : GAME->Find_AssetByGUID(portraitGuid);

    ImGui::Spacing();

    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(120.f);

    const string previewName = selectedAsset != nullptr
        ? Get_AssetFileName(*selectedAsset)
        : string{ "<Select Texture>" };

    const float clearButtonWidth = 52.f;
    const float comboWidth = max(120.f, ImGui::GetContentRegionAvail().x - clearButtonWidth - ImGui::GetStyle().ItemSpacing.x);
    ImGui::SetNextItemWidth(comboWidth);
    if (ImGui::BeginCombo("##PortraitTexturePicker", previewName.c_str()))
    {
        if (_portraitCandidatePrefix != normalizedPrefix)
            Refresh_PortraitTextureCandidates(normalizedPrefix);

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##PortraitTextureSearch", "Search texture...", _portraitSearchBuffer, sizeof(_portraitSearchBuffer));
        ImGui::Separator();

        const string searchText = String::ToLowerCopy(_portraitSearchBuffer);
        Refresh_FilteredPortraitTextureCandidates(searchText);

        if (_filteredPortraitTextureCandidates.empty())
        {
            ImGui::TextDisabled("No portrait textures found.");
        }
        else
        {
            if (ImGui::BeginChild("##PortraitTextureList", ImVec2(520.f, 360.f), true))
            {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int32>(_filteredPortraitTextureCandidates.size()));
                while (clipper.Step())
                {
                    for (int32 rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex)
                    {
                        const PortraitTextureCandidate* candidate = _filteredPortraitTextureCandidates[rowIndex];
                        if (candidate == nullptr)
                            continue;

                        const bool isSelected = portraitGuid == candidate->guid;

                        ImGui::PushID(candidate->guid.c_str());

                        ID3D11ShaderResourceView* previewSrv = Get_PortraitTexturePreviewSRV(candidate->fullPath);
                        if (previewSrv != nullptr)
                            ImGui::Image(reinterpret_cast<ImTextureID>(previewSrv), ImVec2(40.f, 40.f));
                        else
                            ImGui::Dummy(ImVec2(40.f, 40.f));

                        ImGui::SameLine();
                        ImGui::BeginGroup();

                        if (ImGui::Selectable(candidate->fileName.c_str(), isSelected))
                        {
                            portraitGuid = candidate->guid; // prefab에는 사람이 고른 Texture의 GUID만 저장한다.
                            changed = true;
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::TextDisabled("%s", candidate->relativePath.c_str());
                        ImGui::EndGroup();
                        ImGui::Separator();

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();

                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndChild();
        }

        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear##PortraitTexturePicker"))
    {
        portraitGuid.clear();
        changed = true;
    }

    return changed;
}

void CombatStat_Inspector::Refresh_PortraitTextureCandidates(const string& requiredRelativePrefix)
{
    _portraitTextureCandidates.clear();
    _filteredPortraitTextureCandidates.clear();
    _portraitCandidatePrefix = requiredRelativePrefix;
    _lastPortraitSearchText.clear();

    for (const AssetMeta& asset : GAME->Get_AssetsByType("Texture"))
    {
        const string relativePathLower = Normalize_RelativePath(asset);
        if (!requiredRelativePrefix.empty() && !StartsWith(relativePathLower, requiredRelativePrefix))
            continue;

        PortraitTextureCandidate candidate{};
        candidate.guid = asset.guid;
        candidate.fileName = Get_AssetFileName(asset);
        candidate.fileNameLower = String::ToLowerCopy(candidate.fileName);
        candidate.relativePath = String::ToString(asset.relativePath);
        replace(candidate.relativePath.begin(), candidate.relativePath.end(), '\\', '/');
        candidate.relativePathLower = String::ToLowerCopy(candidate.relativePath);
        candidate.fullPath = asset.fullPath;

        _portraitTextureCandidates.push_back(std::move(candidate));
    }
}

void CombatStat_Inspector::Refresh_FilteredPortraitTextureCandidates(const string& searchText)
{
    if (_lastPortraitSearchText == searchText && !_filteredPortraitTextureCandidates.empty())
        return;

    _filteredPortraitTextureCandidates.clear();
    _filteredPortraitTextureCandidates.reserve(_portraitTextureCandidates.size());

    for (const PortraitTextureCandidate& candidate : _portraitTextureCandidates)
    {
        if (!searchText.empty() &&
            candidate.fileNameLower.find(searchText) == string::npos &&
            candidate.relativePathLower.find(searchText) == string::npos)
        {
            continue;
        }

        _filteredPortraitTextureCandidates.push_back(&candidate);
    }

    _lastPortraitSearchText = searchText;
}

ID3D11ShaderResourceView* CombatStat_Inspector::Get_PortraitTexturePreviewSRV(const wstring& texturePath)
{
    if (texturePath.empty())
        return nullptr;

    const fs::path path = fs::path(texturePath).lexically_normal();
    const wstring pathKey = path.wstring();

    const auto cached = _portraitPreviewCache.find(pathKey);
    if (cached != _portraitPreviewCache.end())
        return cached->second.Get();

    if (!fs::exists(path) || !fs::is_regular_file(path))
        return nullptr;

    ID3D11ShaderResourceView* srv = nullptr;
    const fs::path ext = path.extension();

    HRESULT hr = S_OK;
    if (ext == L".dds" || ext == L".DDS")
        hr = DirectX::CreateDDSTextureFromFile(GAME->Get_Device().Get(), path.c_str(), nullptr, &srv);
    else
        hr = DirectX::CreateWICTextureFromFile(GAME->Get_Device().Get(), path.c_str(), nullptr, &srv);

    if (FAILED(hr) || srv == nullptr)
        return nullptr;

    ComPtr<ID3D11ShaderResourceView> cachedSrv{};
    cachedSrv.Attach(srv);

    auto [iter, inserted] = _portraitPreviewCache.emplace(pathKey, std::move(cachedSrv));
    UNREFERENCED_PARAMETER(inserted);

    return iter->second.Get();
}
