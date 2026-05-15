#include "PostProcess_View.h"

PostProcess_View::PostProcess_View()
    : Editor_Window{ L"Post Process" }
{
}

HRESULT PostProcess_View::Initialize()
{
    return Editor_Window::Initialize();
}

void PostProcess_View::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);

    _previewTime += timeDelta;
}

void PostProcess_View::Pre_Render()
{
    Editor_Window::Pre_Render();
}

void PostProcess_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin(windowName.c_str(), &isOpen, flags))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 12.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);

        Draw_StatusBadge();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        Draw_HDRControl();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        Draw_ToneMappingControl();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        Draw_Preview();

        ImGui::PopStyleVar(4);
    }

    Set_Open(isOpen);
    ImGui::End();
}

void PostProcess_View::Draw_StatusBadge()
{
    ImGui::Text("HDR Post Process");
    ImGui::SameLine();

    if (_setting.useHDR)
        ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.45f, 1.f), "ON");
    else
        ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.35f, 1.f), "OFF");

    ImGui::TextDisabled("Scene color, exposure, tone mapping, gamma preview");
}

void PostProcess_View::Draw_HDRControl()
{
    if (ImGui::BeginChild("##HDRControl", ImVec2(0.f, 120.f), true))
    {
        ImGui::Text("HDR");

        // HDR 출력 사용 여부를 조절한다.
        ImGui::Checkbox("Use HDR", &_setting.useHDR);

        ImGui::BeginDisabled(!_setting.useHDR);

        // 카메라 노출값처럼 최종 장면 밝기를 조절한다.
        ImGui::DragFloat("Exposure", &_setting.exposure, 0.01f, 0.01f, 10.f, "%.2f");

        if (ImGui::Button("Dark"))
            _setting.exposure = 0.5f;

        ImGui::SameLine();

        if (ImGui::Button("Default"))
            _setting.exposure = 1.f;

        ImGui::SameLine();

        if (ImGui::Button("Bright"))
            _setting.exposure = 2.f;

        ImGui::EndDisabled();
    }

    ImGui::EndChild();
}

void PostProcess_View::Draw_ToneMappingControl()
{
    if (ImGui::BeginChild("##ToneMappingControl", ImVec2(0.f, 145.f), true))
    {
        ImGui::Text("Tone Mapping");

        // HDR 값을 화면 출력 범위로 변환할지 결정한다.
        ImGui::Checkbox("Use Tone Mapping", &_setting.useToneMapping);

        ImGui::BeginDisabled(!_setting.useHDR || !_setting.useToneMapping);

        const char* toneMappingItems[] =
        {
            "Reinhard",
            "ACES",
            "Linear"
        };

        // 톤 매핑 곡선을 선택한다.
        ImGui::Combo("Type", &_setting.toneMappingType, toneMappingItems, IM_ARRAYSIZE(toneMappingItems));

        // 최종 출력 감마값을 조절한다.
        ImGui::DragFloat("Gamma", &_setting.gamma, 0.01f, 1.f, 3.f, "%.2f");

        ImGui::EndDisabled();

        ImGui::Checkbox("Show Preview", &_setting.showPreview);
    }

    ImGui::EndChild();
}

void PostProcess_View::Draw_Preview()
{
    if (!_setting.showPreview)
        return;

    if (ImGui::BeginChild("##HDRPreview", ImVec2(0.f, 210.f), true))
    {
        ImGui::Text("Preview");

        const ImVec2 previewPos = ImGui::GetCursorScreenPos();
        const float previewWidth = ImGui::GetContentRegionAvail().x;
        const float previewHeight = 72.f;

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const int32 stepCount = 80;
        const float stepWidth = previewWidth / static_cast<float>(stepCount);

        for (int32 i = 0; i < stepCount; ++i)
        {
            const float t0 = static_cast<float>(i) / static_cast<float>(stepCount);
            const float t1 = static_cast<float>(i + 1) / static_cast<float>(stepCount);

            float hdrValue = t0 * 4.f;

            if (_setting.useHDR)
                hdrValue *= _setting.exposure;
            else
                hdrValue = Saturate01(hdrValue);

            float mappedValue = hdrValue;

            if (_setting.useHDR && _setting.useToneMapping)
                mappedValue = Apply_ToneMapping(hdrValue);

            mappedValue = Apply_Gamma(mappedValue);

            const float r = Saturate01(mappedValue * 1.15f);
            const float g = Saturate01(mappedValue * 0.92f);
            const float b = Saturate01(mappedValue * 0.65f);

            const ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1.f));

            const ImVec2 p0 = ImVec2(previewPos.x + stepWidth * i, previewPos.y);
            const ImVec2 p1 = ImVec2(previewPos.x + stepWidth * (i + 1), previewPos.y + previewHeight);

            drawList->AddRectFilled(p0, p1, color);
        }

        drawList->AddRect(previewPos, ImVec2(previewPos.x + previewWidth, previewPos.y + previewHeight), IM_COL32(255, 255, 255, 80), 6.f);

        ImGui::Dummy(ImVec2(previewWidth, previewHeight + 8.f));

        ImGui::TextDisabled("Before");
        ImGui::SameLine(previewWidth - 38.f);
        ImGui::TextDisabled("After");

        ImGui::Spacing();

        const float exposureRatio = Saturate01(_setting.exposure / 10.f);
        const float gammaRatio = Saturate01((_setting.gamma - 1.f) / 2.f);

        // 검사 화면에서 현재 값을 한눈에 보기 위해 막대 형태로 보여준다.
        ImGui::ProgressBar(exposureRatio, ImVec2(-1.f, 0.f), "Exposure");
        ImGui::ProgressBar(gammaRatio, ImVec2(-1.f, 0.f), "Gamma");

        const char* typeName = "ACES";
        if (_setting.toneMappingType == ETOI(ToneMappingType::Reinhard))
            typeName = "Reinhard";
        else if (_setting.toneMappingType == ETOI(ToneMappingType::Linear))
            typeName = "Linear";

        ImGui::Text("Current Type : %s", typeName);
    }

    ImGui::EndChild();
}

float PostProcess_View::Apply_ToneMapping(float value) const
{
    value = max(0.f, value);

    if (_setting.toneMappingType == ETOI(ToneMappingType::Reinhard))
    {
        return value / (value + 1.f);
    }

    if (_setting.toneMappingType == ETOI(ToneMappingType::ACES))
    {
        const float a = 2.51f;
        const float b = 0.03f;
        const float c = 2.43f;
        const float d = 0.59f;
        const float e = 0.14f;

        return Saturate01((value * (a * value + b)) / (value * (c * value + d) + e));
    }

    return Saturate01(value);
}

float PostProcess_View::Apply_Gamma(float value) const
{
    value = Saturate01(value);

    if (_setting.gamma <= 0.001f)
        return value;

    return powf(value, 1.f / _setting.gamma);
}

float PostProcess_View::Saturate01(float value) const
{
    if (value < 0.f)
        return 0.f;

    if (value > 1.f)
        return 1.f;

    return value;
}

Shared<PostProcess_View> PostProcess_View::Create()
{
    return make_shared<PostProcess_View>();
}

void PostProcess_View::Free()
{
    Editor_Window::Free();
}
