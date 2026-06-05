#include "PostProcess_View.h"
#include "PostProcessPipeline.h"
#include "LightingPipeline.h"
#include "GameInstance.h"

PostProcess_View::PostProcess_View()
    : Editor_Window{ L"Post Process" }
{
}

HRESULT PostProcess_View::Initialize()
{
    const HRESULT hr = Editor_Window::Initialize();
    if (FAILED(hr))
        return hr;

    Cache_DefaultSnapshots();
    return S_OK;
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
        Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();

        if (!_hasDefaultSnapshots && pipeline && lightingPipeline)
            Cache_DefaultSnapshots();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 12.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);

        if (!pipeline || !lightingPipeline)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.f), "PostProcess/Lighting pipeline is null");
            ImGui::TextDisabled("Renderer pipeline initialization order check needed.");
        }
        else
        {
            bool dirty = false;
            bool lightingDirty = false;
            PostProcessCB& params = pipeline->Get_PostProcessParams();
            LightShaftCB& lightShaftParams = pipeline->Get_LightShaftParams();

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

            Draw_LightShaftControl(lightShaftParams, dirty);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            Draw_LightingControl(lightingPipeline, lightingDirty);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            Draw_QuickAction(params, lightShaftParams, lightingPipeline, dirty, lightingDirty);

            if (dirty)
            {
                pipeline->Set_PostProcessParams(params);
                pipeline->Set_LightShaftParams(lightShaftParams);
            }

            if (lightingDirty)
            {
                lightingPipeline->Set_Params(lightingPipeline->Get_Params());
                lightingPipeline->Set_DeferredLightingParams(lightingPipeline->Get_DeferredLightingParams());
            }
        }

        ImGui::PopStyleVar(4);
    }

    Set_Open(isOpen);
    ImGui::End();
}

void PostProcess_View::Draw_StatusBadge()
{
    ImGui::Text("Post Process + Lighting");
    ImGui::SameLine();

    Shared<PostProcessPipeline> ppPipeline = GAME->Get_PostProcessPipeline();
    Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();
    const bool ppOn = ppPipeline && ppPipeline->Has_PostProcessFeature(PostProcessFeature::HDR);
    const bool lightingOn = lightingPipeline &&
        (lightingPipeline->Is_Enabled() || lightingPipeline->Is_IBLEnabled());

    if (ppOn || lightingOn)
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
        bool useLightShaft = pipeline->Has_PostProcessFeature(PostProcessFeature::LightShaft);

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
        ImGui::SameLine();
        if (ImGui::Checkbox("LightShaft", &useLightShaft))
        {
            pipeline->Set_PostProcessFeature(PostProcessFeature::LightShaft, useLightShaft);
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
        const char* toneMapItems[] = { "Reinhard", "ACES", "Uncharted2" };
        if (ImGui::Combo("ToneMapMode", &toneMapMode, toneMapItems, IM_ARRAYSIZE(toneMapItems)))
        {
            if (toneMapMode < 0)
                toneMapMode = 0;
            if (toneMapMode >= static_cast<int>(IM_ARRAYSIZE(toneMapItems)))
                toneMapMode = static_cast<int>(IM_ARRAYSIZE(toneMapItems)) - 1;
            params.toneMapMode = static_cast<uint32>(toneMapMode);
            dirty = true;
        }
    }

    ImGui::EndChild();
}

void PostProcess_View::Draw_LightShaftControl(LightShaftCB& params, bool& dirty)
{
    if (ImGui::BeginChild("##LightShaftControl", ImVec2(0.f, 300.f), true))
    {
        ImGui::Text("Light Shaft Params");

        dirty |= ImGui::ColorEdit3("ShaftColor", reinterpret_cast<float*>(&params.shaftColor));
        dirty |= ImGui::DragFloat("Intensity##LightShaft", &params.intensity, 0.01f, 0.f, 8.f, "%.3f");

        int sampleCount = static_cast<int>(params.sampleCount);
        if (ImGui::SliderInt("SampleCount##LightShaft", &sampleCount, 1, 32))
        {
            params.sampleCount = static_cast<uint32>(sampleCount);
            dirty = true;
        }

        dirty |= ImGui::DragFloat("RayLength", &params.rayLength, 0.001f, 0.f, 1.f, "%.3f");
        dirty |= ImGui::DragFloat("Decay", &params.decay, 0.001f, 0.f, 1.f, "%.3f");
        dirty |= ImGui::DragFloat("SunDiscRadius", &params.sunDiscRadius, 0.001f, 0.001f, 0.25f, "%.3f");
        dirty |= ImGui::DragFloat("EdgeFade", &params.edgeFade, 0.001f, 0.f, 0.5f, "%.3f");
        dirty |= ImGui::DragFloat("DepthEmptyThreshold", &params.depthEmptyThreshold, 0.0001f, 0.f, 0.1f, "%.4f");

        ImGui::Separator();
        ImGui::TextDisabled("Runtime values");

        ImGui::BeginDisabled(true);
        ImGui::DragFloat2("LightScreenUV", reinterpret_cast<float*>(&params.lightScreenUV), 0.001f, -1.f, 2.f, "%.3f");
        ImGui::DragFloat("LightVisible", &params.lightVisible, 0.001f, 0.f, 1.f, "%.3f");
        ImGui::DragFloat("LightFacingCamera", &params.lightFacingCamera, 0.001f, 0.f, 1.f, "%.3f");
        ImGui::EndDisabled();
    }

    ImGui::EndChild();
}

void PostProcess_View::Draw_LightingControl(Shared<LightingPipeline> pipeline, bool& dirty)
{
    if (ImGui::BeginChild("##LightingControl", ImVec2(0.f, 460.f), true))
    {
        ImGui::Text("Lighting");

        SSAOCB& ssaoParams = pipeline->Get_Params();
        DeferredLightingCB& lightingParams = pipeline->Get_DeferredLightingParams();

        if (ImGui::CollapsingHeader("SSAO / Core", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool enabled = ssaoParams.SSAOEnabled != 0u;
            if (ImGui::Checkbox("Enable SSAO", &enabled))
            {
                ssaoParams.SSAOEnabled = enabled ? 1u : 0u;
                dirty = true;
            }

            dirty |= ImGui::DragFloat("Radius", &ssaoParams.SSAORadius, 0.001f, 0.01f, 8.f, "%.3f");
            dirty |= ImGui::DragFloat("Bias", &ssaoParams.SSAOBias, 0.001f, 0.f, 0.2f, "%.4f");
            dirty |= ImGui::DragFloat("Intensity", &ssaoParams.SSAOIntensity, 0.01f, 0.f, 4.f, "%.3f");
            dirty |= ImGui::DragFloat("Power", &ssaoParams.SSAOPower, 0.01f, 0.01f, 8.f, "%.3f");
            dirty |= ImGui::DragFloat("DepthRange", &ssaoParams.SSAODepthRange, 0.001f, 0.001f, 4.f, "%.3f");

            int sampleCount = static_cast<int>(ssaoParams.SSAOSampleCount);
            if (ImGui::SliderInt("SampleCount", &sampleCount, 1, 16))
            {
                ssaoParams.SSAOSampleCount = static_cast<uint32>(sampleCount);
                dirty = true;
            }
        }

        if (ImGui::CollapsingHeader("SSAO / Blur", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool bilateral = ssaoParams.SSAOBilateralEnabled != 0u;
            if (ImGui::Checkbox("Bilateral", &bilateral))
            {
                ssaoParams.SSAOBilateralEnabled = bilateral ? 1u : 0u;
                dirty = true;
            }

            dirty |= ImGui::DragFloat("BlurRadius", &ssaoParams.SSAOBlurRadius, 0.1f, 0.0f, 16.f, "%.2f");
            dirty |= ImGui::DragFloat("DepthSharpness", &ssaoParams.SSAODepthSharpness, 0.1f, 0.0f, 400.f, "%.2f");
            dirty |= ImGui::DragFloat("NormalSharpness", &ssaoParams.SSAONormalSharpness, 0.1f, 0.0f, 64.f, "%.2f");
        }

        if (ImGui::CollapsingHeader("IBL / Deferred", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool iblEnabled = lightingParams.iblEnabled != 0u;
            if (ImGui::Checkbox("Enable IBL", &iblEnabled))
            {
                lightingParams.iblEnabled = iblEnabled ? 1u : 0u;
                dirty = true;
            }

            dirty |= ImGui::DragFloat("IBL Intensity", &lightingParams.iblIntensity, 0.01f, 0.f, 8.f, "%.2f");
            dirty |= ImGui::DragFloat("IBL Diffuse Intensity", &lightingParams.iblDiffuseIntensity, 0.01f, 0.f, 8.f, "%.2f");
            dirty |= ImGui::DragFloat("IBL Specular Intensity", &lightingParams.iblSpecularIntensity, 0.01f, 0.f, 8.f, "%.2f");
            dirty |= ImGui::DragFloat("IBL Prefilter MaxMip", &lightingParams.iblPrefilterMaxMip, 0.1f, 0.f, 16.f, "%.2f");
        }

        if (ImGui::CollapsingHeader("Specular AA / Deferred", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool specAAEnabled = lightingParams.specAAEnabled != 0u;
            if (ImGui::Checkbox("Enable Spec AA", &specAAEnabled))
            {
                lightingParams.specAAEnabled = specAAEnabled ? 1u : 0u;
                dirty = true;
            }

            dirty |= ImGui::DragFloat("Spec AA Strength", &lightingParams.specAAStrength, 0.01f, 0.f, 4.f, "%.3f");
            dirty |= ImGui::DragFloat("Spec AA Min Variance", &lightingParams.specAAMinVariance, 0.001f, 0.f, 1.f, "%.4f");
            dirty |= ImGui::DragFloat("Spec AA Max Added Roughness", &lightingParams.specAAMaxAddedRoughness, 0.01f, 0.f, 1.f, "%.3f");
        }
    }

    ImGui::EndChild();
}

void PostProcess_View::Draw_QuickAction(PostProcessCB& params, LightShaftCB& lightShaftParams, Shared<LightingPipeline> lightingPipeline, bool& dirty, bool& lightingDirty)
{
    if (ImGui::BeginChild("##QuickAction", ImVec2(0.f, 140.f), true))
    {
        ImGui::Text("Quick Actions");

        if (ImGui::Button("Reset Parameters"))
        {
            params = _defaultPostProcessParams;
            lightShaftParams = _defaultLightShaftParams;
            dirty = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Reset Lighting Params") && lightingPipeline)
        {
            lightingPipeline->Get_Params() = _defaultSSAOParams;
            lightingPipeline->Get_DeferredLightingParams() = _defaultDeferredLightingParams;
            lightingDirty = true;
        }

        if (ImGui::Button("Reload Renderer Shaders"))
        {
            const HRESULT hr = GAME->Reload_RendererShaders();
            if (hr == S_OK)
            {
                _reloadShaderStatusText = "Reloaded Deferred/PostProcess/ScreenFx shaders";
                _reloadShaderStatusIsError = false;
            }
            else if (hr == S_FALSE)
            {
                _reloadShaderStatusText = "Renderer shader reload partially succeeded; failed shaders kept previous version";
                _reloadShaderStatusIsError = true;
            }
            else
            {
                _reloadShaderStatusText = "Renderer shader reload failed; previous shaders kept";
                _reloadShaderStatusIsError = true;
            }
        }

        if (ImGui::Button("Reload ShaderComs"))
        {
            const HRESULT hr = GAME->Reload_ShaderComs();
            if (hr == S_OK)
            {
                _reloadShaderComStatusText = "Reloaded Shader_VtxMesh.hlsl and Shader_VtxAnimMesh.hlsl";
                _reloadShaderComStatusIsError = false;
            }
            else if (hr == S_FALSE)
            {
                _reloadShaderComStatusText = "ShaderCom reload partially succeeded; some shaders kept previous versions";
                _reloadShaderComStatusIsError = true;
            }
            else
            {
                _reloadShaderComStatusText = "ShaderCom reload failed; previous shaders kept";
                _reloadShaderComStatusIsError = true;
            }
        }

        if (!_reloadShaderStatusText.empty())
        {
            const ImVec4 statusColor = _reloadShaderStatusIsError
                ? ImVec4(0.95f, 0.8f, 0.35f, 1.f)
                : ImVec4(0.35f, 0.9f, 0.45f, 1.f);
            ImGui::TextColored(statusColor, "%s", _reloadShaderStatusText.c_str());
        }

        if (!_reloadShaderComStatusText.empty())
        {
            const ImVec4 statusColor = _reloadShaderComStatusIsError
                ? ImVec4(0.95f, 0.8f, 0.35f, 1.f)
                : ImVec4(0.35f, 0.9f, 0.45f, 1.f);
            ImGui::TextColored(statusColor, "%s", _reloadShaderComStatusText.c_str());
        }
    }

    ImGui::EndChild();
}

void PostProcess_View::Cache_DefaultSnapshots()
{
    Shared<PostProcessPipeline> pipeline = GAME->Get_PostProcessPipeline();
    Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();
    if (!pipeline || !lightingPipeline)
        return;

    _defaultPostProcessParams = pipeline->Get_PostProcessParams();
    _defaultLightShaftParams = pipeline->Get_LightShaftParams();
    _defaultSSAOParams = lightingPipeline->Get_Params();
    _defaultDeferredLightingParams = lightingPipeline->Get_DeferredLightingParams();
    _hasDefaultSnapshots = true;
}

Shared<PostProcess_View> PostProcess_View::Create()
{
    return make_shared<PostProcess_View>();
}

void PostProcess_View::Free()
{
    Editor_Window::Free();
}
