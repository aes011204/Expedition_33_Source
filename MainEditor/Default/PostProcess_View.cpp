#include "PostProcess_View.h"
#include "PostProcessPipeline.h"
#include "GameInstance.h"

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
        Shared<PostProcessPipeline> pipeline = GAME->Get_PostProcessPipeline();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 12.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);

        if (!pipeline)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.f), "PostProcessPipeline is null");
            ImGui::TextDisabled("Renderer/PostProcessPipeline initialization order check needed.");
        }
        else
        {
            bool dirty = false;
            PostProcessCB& params = pipeline->Get_PostProcessParams();

            Draw_StatusBadge();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            Draw_FeatureControl(pipeline, dirty);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            Draw_ParameterControl(params, dirty);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            Draw_QuickAction(params, dirty);

            if (dirty)
                pipeline->Set_PostProcessParams(params);
        }

        ImGui::PopStyleVar(4);
    }

    Set_Open(isOpen);
    ImGui::End();
}

void PostProcess_View::Draw_StatusBadge()
{
    ImGui::Text("HDR Post Process");
    ImGui::SameLine();

    Shared<PostProcessPipeline> pipeline = GAME->Get_PostProcessPipeline();
    if (pipeline && pipeline->Has_PostProcessFeature(PostProcessFeature::HDR))
        ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.45f, 1.f), "ON");
    else
        ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.35f, 1.f), "OFF");

    ImGui::TextDisabled("Feature + parameter changes are reflected in real time");
}

void PostProcess_View::Draw_FeatureControl(Shared<PostProcessPipeline> pipeline, bool& dirty)
{
    if (ImGui::BeginChild("##FeatureControl", ImVec2(0.f, 130.f), true))
    {
        ImGui::Text("Feature Flags");

        bool useHDR = pipeline->Has_PostProcessFeature(PostProcessFeature::HDR);
        bool useToneMapping = pipeline->Has_PostProcessFeature(PostProcessFeature::ToneMapping);
        bool useExposure = pipeline->Has_PostProcessFeature(PostProcessFeature::Exposure);
        bool useBloom = pipeline->Has_PostProcessFeature(PostProcessFeature::Bloom);
        bool useGamma = pipeline->Has_PostProcessFeature(PostProcessFeature::Gamma);

        if (ImGui::Checkbox("HDR", &useHDR))
        {
            pipeline->Set_PostProcessFeature(PostProcessFeature::HDR, useHDR);
            dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("ToneMapping", &useToneMapping))
        {
            pipeline->Set_PostProcessFeature(PostProcessFeature::ToneMapping, useToneMapping);
            dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Exposure", &useExposure))
        {
            pipeline->Set_PostProcessFeature(PostProcessFeature::Exposure, useExposure);
            dirty = true;
        }

        if (ImGui::Checkbox("Bloom", &useBloom))
        {
            pipeline->Set_PostProcessFeature(PostProcessFeature::Bloom, useBloom);
            dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Gamma", &useGamma))
        {
            pipeline->Set_PostProcessFeature(PostProcessFeature::Gamma, useGamma);
            dirty = true;
        }
    }

    ImGui::EndChild();
}

void PostProcess_View::Draw_ParameterControl(PostProcessCB& params, bool& dirty)
{
    if (ImGui::BeginChild("##ParamControl", ImVec2(0.f, 260.f), true))
    {
        ImGui::Text("PostProcess Params");

        dirty |= ImGui::DragFloat("Exposure", &params.exposure, 0.01f, 0.f, 10.f, "%.2f");
        dirty |= ImGui::DragFloat("InvWhitePoint", &params.invWhitePoint, 0.01f, 0.01f, 10.f, "%.2f");
        dirty |= ImGui::DragFloat("Gamma", &params.gamma, 0.01f, 1.f, 3.f, "%.2f");
        dirty |= ImGui::DragFloat("BloomRadius", &params.bloomRadius, 0.01f, 0.f, 10.f, "%.2f");

        dirty |= ImGui::DragFloat("BloomThreshold", &params.bloomThreshold, 0.01f, 0.f, 10.f, "%.2f");
        dirty |= ImGui::DragFloat("BloomIntensity", &params.bloomIntensity, 0.01f, 0.f, 5.f, "%.2f");
        dirty |= ImGui::DragFloat("BloomSoftKnee", &params.bloomSoftKnee, 0.01f, 0.f, 1.f, "%.2f");
        dirty |= ImGui::DragFloat("BloomClamp", &params.bloomClamp, 0.05f, 0.f, 50.f, "%.2f");
        dirty |= ImGui::DragFloat("BloomScatter", &params.bloomScatter, 0.01f, 0.f, 1.f, "%.2f");

        int toneMapMode = static_cast<int>(params.toneMapMode);
        const char* toneMapItems[] = { "Reinhard", "ACES", "Linear" };
        if (ImGui::Combo("ToneMapMode", &toneMapMode, toneMapItems, IM_ARRAYSIZE(toneMapItems)))
        {
            if (toneMapMode < 0)
                toneMapMode = 0;
            params.toneMapMode = static_cast<uint32>(toneMapMode);
            dirty = true;
        }

        float texelSize[2] = { params.bloomTexelSize.x, params.bloomTexelSize.y };
        if (ImGui::DragFloat2("BloomTexelSize", texelSize, 0.00001f, 0.f, 0.1f, "%.6f"))
        {
            params.bloomTexelSize.x = texelSize[0];
            params.bloomTexelSize.y = texelSize[1];
            dirty = true;
        }
    }

    ImGui::EndChild();
}

void PostProcess_View::Draw_QuickAction(PostProcessCB& params, bool& dirty)
{
    if (ImGui::BeginChild("##QuickAction", ImVec2(0.f, 70.f), true))
    {
        ImGui::Text("Quick Actions");

        if (ImGui::Button("Reset Parameters"))
        {
            params = Make_DefaultParams();
            dirty = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Auto BloomTexel from Viewport"))
        {
            const float width = static_cast<float>(GAME->Get_ViewportWidth());
            const float height = static_cast<float>(GAME->Get_ViewportHeight());

            if (width > 0.f && height > 0.f)
            {
                params.bloomTexelSize = Vec2(1.f / width, 1.f / height);
                dirty = true;
            }
        }
    }

    ImGui::EndChild();
}

PostProcessCB PostProcess_View::Make_DefaultParams()
{
    PostProcessCB defaults{};
    defaults.exposure = 1.f;
    defaults.invWhitePoint = 1.f;
    defaults.gamma = 2.2f;
    defaults.bloomRadius = 1.f;
    defaults.bloomThreshold = 1.f;
    defaults.bloomIntensity = 0.5f;
    defaults.bloomSoftKnee = 0.5f;
    defaults.bloomClamp = 10.f;
    defaults.bloomScatter = 0.7f;
    defaults.toneMapMode = 0;
    defaults.bloomTexelSize = Vec2(0.f, 0.f);
    return defaults;
}

Shared<PostProcess_View> PostProcess_View::Create()
{
    return make_shared<PostProcess_View>();
}

void PostProcess_View::Free()
{
    Editor_Window::Free();
}
