#include "pch.h"
#include "PanelAssetBrowser.h"

PanelAssetBrowser::PanelAssetBrowser()
    : MEWindow{ L"Asset Browser" }
{

}

void PanelAssetBrowser::Render()
{
    bool open = IsOpen();
    if (ImGui::Begin("Asset Browser", &open))
    {
        ImGui::Text("맵 에셋 브라우저 - 준비 중");
    }
    ImGui::End();
    SetOpen(open);
}

Shared<PanelAssetBrowser> PanelAssetBrowser::Create()
{
    return make_shared<PanelAssetBrowser>();
}
