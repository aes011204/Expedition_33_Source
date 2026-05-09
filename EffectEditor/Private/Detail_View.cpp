#include "Detail_View.h"

NS_BEGIN(EffectEditor)

Detail_View::Detail_View()
    : Editor_Window{ L"Detail", ICON_FA_PEN_TO_SQUARE }
{
}

Detail_View::~Detail_View()
{
    Free();
}

void Detail_View::Update(float timeDelta)
{
}

void Detail_View::Render()
{
    if (!Is_Open())
        return;

    bool bOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &bOpen))
    {
        ImGui::TextUnformatted("No Selected");
        ImGui::Separator();
        ImGui::TextDisabled("Add Component");
    }
    Set_Open(bOpen);
    ImGui::End();
}

Shared<Detail_View> Detail_View::Create()
{
    return make_shared<Detail_View>();
}

void Detail_View::Free()
{
    __super::Free();
}

NS_END
