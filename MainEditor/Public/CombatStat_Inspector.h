#pragma once
#include "Component_Inspector.h"

NS_BEGIN(Client)
class CombatStatCom;
NS_END

NS_BEGIN(MainEditor)

class CombatStat_Inspector : public Component_Inspector
{
public:
    CombatStat_Inspector() = default;
    ~CombatStat_Inspector() override = default;

protected:
    void Draw_CommonRuntimeSection(const Shared<Client::CombatStatCom>& stat);

    bool Draw_PortraitTexturePicker(
        const char* label,
        string& portraitGuid,
        const string& requiredRelativePrefix);

private:
    struct PortraitTextureCandidate
    {
        string guid{};
        string fileName{};
        string fileNameLower{};
        string relativePath{};
        string relativePathLower{};
        wstring fullPath{};
    };

    void Refresh_PortraitTextureCandidates(const string& requiredRelativePrefix);
    void Refresh_FilteredPortraitTextureCandidates(const string& searchText);
    ID3D11ShaderResourceView* Get_PortraitTexturePreviewSRV(const wstring& texturePath);

private:
    vector<PortraitTextureCandidate>        _portraitTextureCandidates{};
    vector<const PortraitTextureCandidate*> _filteredPortraitTextureCandidates{};
    umap<wstring, ComPtr<ID3D11ShaderResourceView>> _portraitPreviewCache{};
    string                                  _portraitCandidatePrefix{};
    string                                  _lastPortraitSearchText{};
    char                                    _portraitSearchBuffer[128]{};
};

NS_END
