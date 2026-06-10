#include "pch.h"
#include "ModelTestPanel.h"
#include <imgui.h>
#include "ModelImGui_Manager.h"
#include "ModelViewerObject.h"
#include "ModelCom.h"
#include "TransformCom.h"
#include "CurveClip.h"
#include "CurveAnimationModifier.h"
#include <commdlg.h>
#include <cmath>

#pragma comment(lib, "Comdlg32.lib")

NS_BEGIN(ModelEditor)

namespace
{
    constexpr wchar_t CURVE_CLIP_BINARY_FILTER[] =
        L"CurveClip Binary (*.curveclip)\0*.curveclip\0All Files (*.*)\0*.*\0";
    constexpr wchar_t CURVE_JSON_FILTER[] =
        L"UE Curve JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    constexpr wchar_t POSE_ASSET_JSON_EXTENSION[] = L".json";

    constexpr char JSON_NAME_KEY[] = "Name";
    constexpr char JSON_TYPE_KEY[] = "Type";
    constexpr char JSON_PROPERTIES_KEY[] = "Properties";
    constexpr char JSON_ANIM_SEQUENCE_TYPE[] = "AnimSequence";
    constexpr char JSON_POSE_CONTAINER_KEY[] = "PoseContainer";
    constexpr char JSON_POSE_NAMES_KEY[] = "PoseFNames";
    constexpr char JSON_POSES_KEY[] = "Poses";
    constexpr char JSON_CURVES_KEY[] = "Curves";
    constexpr char JSON_CURVE_NAME_KEY[] = "CurveName";
    constexpr char JSON_CURVE_DATA_KEY[] = "CurveData";
    constexpr char JSON_COMPRESSED_CURVE_DATA_KEY[] = "CompressedCurveData";
    constexpr char JSON_FLOAT_CURVES_KEY[] = "FloatCurves";
    constexpr char JSON_FLOAT_CURVE_KEY[] = "FloatCurve";
    constexpr char JSON_KEYS_KEY[] = "Keys";
    constexpr char JSON_VALUE_KEY[] = "Value";

    constexpr float ZERO_CURVE_EPSILON = 0.000001f;

    bool Has_FileExtension(const fs::path& path, const wchar_t* expectedExtension)
    {
        const string actual = String::ToLowerCopy(String::ToString(path.extension().wstring()));
        const string expected = String::ToLowerCopy(String::ToString(expectedExtension));
        return actual == expected;
    }

    /* CurveClip binary writer used only by ModelEditor build tools */
    void Write_CurveClipString(ofstream& out, const string& value)
    {
        const uint32 length = static_cast<uint32>(value.size());
        out.write(reinterpret_cast<const char*>(&length), sizeof(uint32));
        if (length > 0)
            out.write(value.data(), length);
    }

    void Write_CurveClipCurve(ofstream& out, const CurveClip::Curve& curve)
    {
        Write_CurveClipString(out, curve.name);
        out.write(reinterpret_cast<const char*>(&curve.value), sizeof(float));
    }

    void Write_CurveClipPose(ofstream& out, const CurveClip::CurveClip_Pose& pose)
    {
        const uint32 curveCount = static_cast<uint32>(pose.curves.size());
        Write_CurveClipString(out, pose.name);
        out.write(reinterpret_cast<const char*>(&curveCount), sizeof(uint32));

        for (const CurveClip::Curve& curve : pose.curves)
            Write_CurveClipCurve(out, curve);
    }

    HRESULT Save_CurveClipBinary(const CurveClip& clip, const fs::path& outputPath)
    {
        if (clip.Get_Poses().empty())
            return E_FAIL;

        ofstream out(outputPath, ios::binary);
        if (!out.is_open())
            return E_FAIL;

        const uint32 poseCount = static_cast<uint32>(clip.Get_Poses().size());
        const uint32 magic = CurveClipFormat::MAGIC;
        const uint32 version = CurveClipFormat::VERSION;
        out.write(reinterpret_cast<const char*>(&magic), sizeof(uint32));
        out.write(reinterpret_cast<const char*>(&version), sizeof(uint32));
        out.write(reinterpret_cast<const char*>(&poseCount), sizeof(uint32));

        Write_CurveClipString(out, clip.Get_SourceName());
        Write_CurveClipString(out, clip.Get_SourcePath());

        for (const CurveClip::CurveClip_Pose& pose : clip.Get_Poses())
            Write_CurveClipPose(out, pose);

        return out.good() ? S_OK : E_FAIL;
    }

    /* UE PoseAsset JSON discovery */
    const json* Find_PoseContainer(const json& root)
    {
        auto tryGetContainer = [](const json& node) -> const json*
        {
            if (node.contains(JSON_POSE_CONTAINER_KEY) && node[JSON_POSE_CONTAINER_KEY].is_object())
                return &node[JSON_POSE_CONTAINER_KEY];

            if (node.contains(JSON_PROPERTIES_KEY) &&
                node[JSON_PROPERTIES_KEY].contains(JSON_POSE_CONTAINER_KEY) &&
                node[JSON_PROPERTIES_KEY][JSON_POSE_CONTAINER_KEY].is_object())
            {
                return &node[JSON_PROPERTIES_KEY][JSON_POSE_CONTAINER_KEY];
            }

            return nullptr;
        };

        if (const json* container = tryGetContainer(root))
            return container;

        if (root.is_array())
        {
            for (const json& item : root)
            {
                if (const json* container = tryGetContainer(item))
                    return container;
            }
        }

        return nullptr;
    }

    /* UE JSON name helpers */
    string Read_JsonName(const json& node, const char* key)
    {
        if (node.is_string())
            return node.get<string>();

        if (node.is_object() && node.contains(key) && node[key].is_string())
            return node[key].get<string>();

        return {};
    }

    string Read_PoseAssetSourceName(const json& root, const fs::path& filePath)
    {
        if (root.is_array() && !root.empty())
            return root.front().value(JSON_NAME_KEY, filePath.stem().string());

        return root.value(JSON_NAME_KEY, filePath.stem().string());
    }

    vector<string> Read_PoseAssetCurveNames(const json& curves)
    {
        vector<string> curveNames{};
        curveNames.reserve(curves.size());

        for (const json& curveNode : curves)
            curveNames.push_back(Read_JsonName(curveNode, JSON_CURVE_NAME_KEY));

        return curveNames;
    }

    CurveClip::CurveClip_Pose Build_PoseAssetPose(
        const json& poseNameNode,
        const json& poseNode,
        const vector<string>& curveNames)
    {
        CurveClip::CurveClip_Pose pose{};
        pose.name = poseNameNode.is_string()
            ? poseNameNode.get<string>()
            : Read_JsonName(poseNameNode, JSON_NAME_KEY);

        const json& curveData = poseNode.contains(JSON_CURVE_DATA_KEY)
            ? poseNode[JSON_CURVE_DATA_KEY]
            : json::array();

        if (!curveData.is_array())
            return pose;

        const size_t curveCount = std::min(curveData.size(), curveNames.size());
        pose.curves.reserve(curveCount);

        for (size_t curveIndex = 0; curveIndex < curveCount; ++curveIndex)
        {
            if (curveNames[curveIndex].empty() || !curveData[curveIndex].is_number())
                continue;

            const float value = curveData[curveIndex].get<float>();
            if (std::abs(value) <= ZERO_CURVE_EPSILON)
                continue;

            pose.curves.push_back(CurveClip::Curve{ curveNames[curveIndex], value });
        }

        return pose;
    }

    vector<CurveClip::CurveClip_Pose> Build_PoseAssetPoses(
        const json& poseNames,
        const json& poses,
        const vector<string>& curveNames)
    {
        vector<CurveClip::CurveClip_Pose> clipPoses{};
        const size_t poseCount = std::min(poseNames.size(), poses.size());
        clipPoses.reserve(poseCount);

        for (size_t poseIndex = 0; poseIndex < poseCount; ++poseIndex)
        {
            clipPoses.push_back(
                Build_PoseAssetPose(poseNames[poseIndex], poses[poseIndex], curveNames));
        }

        return clipPoses;
    }

    /* UE AnimSequence compressed curve discovery */
    const json* Find_AnimSequenceWithCurveData(const json& root)
    {
        auto tryGetAnimSequence = [](const json& node) -> const json*
        {
            if (!node.is_object() || node.value(JSON_TYPE_KEY, string{}) != JSON_ANIM_SEQUENCE_TYPE)
                return nullptr;

            if (!node.contains(JSON_COMPRESSED_CURVE_DATA_KEY) ||
                !node[JSON_COMPRESSED_CURVE_DATA_KEY].contains(JSON_FLOAT_CURVES_KEY) ||
                !node[JSON_COMPRESSED_CURVE_DATA_KEY][JSON_FLOAT_CURVES_KEY].is_array())
            {
                return nullptr;
            }

            return &node;
        };

        if (const json* animSequence = tryGetAnimSequence(root))
            return animSequence;

        if (root.is_array())
        {
            for (const json& item : root)
            {
                if (const json* animSequence = tryGetAnimSequence(item))
                    return animSequence;
            }
        }

        return nullptr;
    }

    float Read_StrongestAnimCurveValue(const json& curveNode)
    {
        if (!curveNode.contains(JSON_FLOAT_CURVE_KEY) ||
            !curveNode[JSON_FLOAT_CURVE_KEY].contains(JSON_KEYS_KEY) ||
            !curveNode[JSON_FLOAT_CURVE_KEY][JSON_KEYS_KEY].is_array())
        {
            return 0.f;
        }

        const json& keys = curveNode[JSON_FLOAT_CURVE_KEY][JSON_KEYS_KEY];
        float strongestValue = 0.f;
        for (const json& key : keys)
        {
            if (!key.contains(JSON_VALUE_KEY) || !key[JSON_VALUE_KEY].is_number())
                continue;

            const float value = key[JSON_VALUE_KEY].get<float>();
            if (std::abs(value) > std::abs(strongestValue))
                strongestValue = value;
        }

        return strongestValue;
    }

    vector<CurveClip::CurveClip_Pose> Build_AnimSequenceCompressedCurvePose(
        const json& animSequenceNode,
        const fs::path& filePath)
    {
        // Current CurveClip stores pose snapshots, so AnimSequence curves are baked as strongest-value poses.
        const json& floatCurves =
            animSequenceNode[JSON_COMPRESSED_CURVE_DATA_KEY][JSON_FLOAT_CURVES_KEY];

        CurveClip::CurveClip_Pose pose{};
        pose.name = animSequenceNode.value(JSON_NAME_KEY, filePath.stem().string());
        pose.curves.reserve(floatCurves.size());

        for (const json& curveNode : floatCurves)
        {
            const string curveName = curveNode.value(JSON_CURVE_NAME_KEY, string{});
            if (curveName.empty())
                continue;

            const float value = Read_StrongestAnimCurveValue(curveNode);
            if (std::abs(value) <= ZERO_CURVE_EPSILON)
                continue;

            pose.curves.push_back(CurveClip::Curve{ curveName, value });
        }

        vector<CurveClip::CurveClip_Pose> poses{};
        poses.push_back(std::move(pose));
        return poses;
    }

    /* Editor-only JSON -> runtime binary source conversion */
    HRESULT Load_CurveJsonForBuild(const fs::path& filePath, CurveClip& outClip)
    {
        ifstream in(filePath);
        if (!in.is_open())
            return E_FAIL;

        json root = json::parse(in, nullptr, false);
        if (root.is_discarded())
            return E_FAIL;

        if (const json* poseContainer = Find_PoseContainer(root))
        {
            const json& poseNames = (*poseContainer)[JSON_POSE_NAMES_KEY];
            const json& poses = (*poseContainer)[JSON_POSES_KEY];
            const json& curves = (*poseContainer)[JSON_CURVES_KEY];
            if (!poseNames.is_array() || !poses.is_array() || !curves.is_array())
                return E_FAIL;

            return outClip.Set_Data(
                Read_PoseAssetSourceName(root, filePath),
                String::ToString(filePath.wstring()),
                Build_PoseAssetPoses(poseNames, poses, Read_PoseAssetCurveNames(curves)));
        }

        if (const json* animSequence = Find_AnimSequenceWithCurveData(root))
        {
            return outClip.Set_Data(
                animSequence->value(JSON_NAME_KEY, filePath.stem().string()),
                String::ToString(filePath.wstring()),
                Build_AnimSequenceCompressedCurvePose(*animSequence, filePath));
        }

        return E_FAIL;
    }
}

ModelTestPanel::ModelTestPanel() : EditorBlock{}
{
    strcpy_s(_curveModifierNameBuffer, DEFAULT_CURVE_MODIFIER_TAG);
}

HRESULT ModelTestPanel::Initialize()
{
	return S_OK;
}

void ModelTestPanel::Update(float timeDelta)
{

}

void ModelTestPanel::Render()
{
	if (!_showBlock) return;

	ImGui::Begin("Model Test Panel");

    Render_MorphTargetTest();
    ImGui::Separator();
    Render_CurveClipTest();
    ImGui::Separator();
    Render_MotionWarpingTest();

	ImGui::End();
}

void ModelTestPanel::Render_MorphTargetTest()
{
    ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "Morph Target Test");
    ImGui::Separator();

    auto modelCom = Get_CurrentModelComponent();
    if (modelCom == nullptr)
    {
        ImGui::TextDisabled("No model loaded.");
        return;
    }

    const MorphData* morphData = modelCom->Get_MorphData();
    if (morphData == nullptr)
    {
        ImGui::TextDisabled("No .morph loaded.");
        return;
    }

    ImGui::Text("Targets: %u", static_cast<uint32>(morphData->targets.size()));
    ImGui::Text("Meshes: %u", static_cast<uint32>(morphData->meshes.size()));

    auto viewer = ModelImGui_Manager::GetInstance()->Get_CurrentModelObject();
    ImGui::Text("Compute Preview: %s / Last Render: %s",
        viewer != nullptr && viewer->Is_ComputePreviewReady() ? "Ready" : "Disabled",
        viewer != nullptr && viewer->Was_LastRenderComputeSkinned() ? "Compute" : "Default");
    ImGui::InputText("Filter##MorphTargetFilter", _morphFilter, sizeof(_morphFilter));

    const string filter = _morphFilter;
    ImGui::BeginChild("MorphTargetList", ImVec2(0.f, 180.f), true);
    for (uint32 i = 0; i < morphData->targets.size(); ++i)
    {
        const string& targetName = morphData->targets[i];
        if (!filter.empty() && targetName.find(filter) == string::npos)
            continue;

        const bool selected = (_selectedMorphIndex == static_cast<int32>(i));
        if (ImGui::Selectable((targetName + "##" + to_string(i)).c_str(), selected))
        {
            _selectedMorphIndex = static_cast<int32>(i);
            _selectedMorphWeight = 1.f;
            Disable_CurveModifier();
            modelCom->Reset_MorphWeights();
            modelCom->Set_MorphWeight(i, _selectedMorphWeight);
        }
    }
    ImGui::EndChild();

    if (_selectedMorphIndex >= 0 && _selectedMorphIndex < static_cast<int32>(morphData->targets.size()))
    {
        const string& selectedName = morphData->targets[_selectedMorphIndex];
        ImGui::TextWrapped("Selected: %s", selectedName.c_str());

        Render_SelectedMorphDebug(*morphData, modelCom->Get_MorphWeights());

        if (ImGui::SliderFloat("Weight##MorphTargetWeight", &_selectedMorphWeight, 0.f, 1.f))
        {
            Disable_CurveModifier();
            modelCom->Reset_MorphWeights();
            modelCom->Set_MorphWeight(static_cast<uint32>(_selectedMorphIndex), _selectedMorphWeight);
        }
    }

    if (ImGui::Button("Reset Morph", ImVec2(-1.f, 0.f)))
    {
        _selectedMorphWeight = 0.f;
        Disable_CurveModifier();
        modelCom->Reset_MorphWeights();
    }
}

void ModelTestPanel::Render_SelectedMorphDebug(const Engine::MorphData& morphData, const vector<float>* weights) const
{
    const uint32 targetIndex = static_cast<uint32>(_selectedMorphIndex);
    float currentWeight = 0.f;
    if (weights != nullptr && targetIndex < weights->size())
        currentWeight = (*weights)[targetIndex];

    uint32 activeMeshCount = 0;
    uint32 totalDeltaCount = 0;
    float maxDeltaLength = 0.f;
    float deltaLengthSum = 0.f;

    for (const MorphMeshData& meshData : morphData.meshes)
    {
        if (targetIndex >= meshData.vertexRanges.size())
            continue;

        const MorphVertexRange& range = meshData.vertexRanges[targetIndex];
        if (range.count == 0 || range.start >= meshData.vertexDeltas.size())
            continue;

        ++activeMeshCount;
        const uint32 rangeEnd = range.start + std::min<uint32>(
            range.count,
            static_cast<uint32>(meshData.vertexDeltas.size()) - range.start);

        for (uint32 deltaIndex = range.start; deltaIndex < rangeEnd; ++deltaIndex)
        {
            const MorphVertexDelta& delta = meshData.vertexDeltas[deltaIndex];
            const Vec3& offset = delta.deltaPosition;
            const float length =
                std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z) *
                std::abs(delta.weightScale);

            maxDeltaLength = std::max(maxDeltaLength, length);
            deltaLengthSum += length;
            ++totalDeltaCount;
        }
    }

    const float avgDeltaLength = totalDeltaCount > 0
        ? deltaLengthSum / static_cast<float>(totalDeltaCount)
        : 0.f;

    ImGui::Text("Current Weight: %.3f", currentWeight);
    ImGui::Text("Target Deltas: %u / Meshes: %u", totalDeltaCount, activeMeshCount);
    ImGui::Text("Delta Length: max %.4f / avg %.4f", maxDeltaLength, avgDeltaLength);
}

void ModelTestPanel::Render_CurveClipTest()
{
    ImGui::TextColored(ImVec4(0.55f, 0.95f, 0.75f, 1.0f), "Curve Clip Test");
    ImGui::Separator();

    Render_CurveClipSourceControls();

    if (!_curveClipStatus.empty())
        ImGui::TextWrapped("%s", _curveClipStatus.c_str());

    auto modelCom = Get_CurrentModelComponent();
    if (modelCom == nullptr || modelCom->Get_MorphData() == nullptr)
    {
        ImGui::TextDisabled("CurveClip build/load is available. Load a model with curve targets to apply it.");
    }

    if (_curveClip == nullptr)
        return;

    ImGui::Text("Loaded: %s", _curveClip->Get_SourceName().c_str());
    ImGui::Text("Poses: %u", static_cast<uint32>(_curveClip->Get_Poses().size()));

    Render_CurveClipPoseList();
    Render_SelectedCurveClipPose();

    if (ImGui::Button("Disable CurveClip Modifier", ImVec2(-1.f, 0.f)))
        Disable_CurveModifier();
}

Engine::ModelCom* ModelTestPanel::Get_CurrentModelComponent() const
{
    auto viewer = ModelImGui_Manager::GetInstance()->Get_CurrentModelObject();
    return viewer != nullptr ? viewer->Get_ModelComponent().get() : nullptr;
}

void ModelTestPanel::Render_CurveClipSourceControls()
{
    ImGui::Text("Modifier");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##CurveModifierName", _curveModifierNameBuffer, sizeof(_curveModifierNameBuffer));

    ImGui::Text("Curve JSON");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-85.f);
    ImGui::InputText("##CurveJsonPath", _curveJsonPathBuffer, sizeof(_curveJsonPathBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Browse##CurveJsonPath", ImVec2(75.f, 0.f)))
    {
        if (Browse_File(CURVE_JSON_FILTER, _curveJsonPathBuffer, sizeof(_curveJsonPathBuffer)))
            Build_SelectedCurveClipJson();
    }

    if (ImGui::Button("Build Selected JSON", ImVec2(-1.f, 0.f)))
        Build_SelectedCurveClipJson();

    ImGui::Text("CurveClip");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-85.f);
    ImGui::InputText("##CurveClipPath", _curveClipPathBuffer, sizeof(_curveClipPathBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Browse##CurveClipPath", ImVec2(75.f, 0.f)))
        Browse_File(CURVE_CLIP_BINARY_FILTER, _curveClipPathBuffer, sizeof(_curveClipPathBuffer));

    if (ImGui::Button("Build Modifier Preset From Binaries", ImVec2(-1.f, 0.f)))
        Build_CurvePresetFromCurrentModelFolder();

    if (ImGui::Button("Load Modifier Preset", ImVec2(-1.f, 0.f)))
        Apply_CurvePresetModifier();

    if (ImGui::Button("Load CurveClip", ImVec2(-1.f, 0.f)))
        Load_CurveClip();

    if (ImGui::Button("Load + Apply CurveClip", ImVec2(-1.f, 0.f)))
    {
        if (SUCCEEDED(Load_CurveClip()))
            Apply_CurveClipPose();
    }
}

void ModelTestPanel::Render_CurveClipPoseList()
{
    const auto& poses = _curveClip->Get_Poses();
    ImGui::InputText("Filter##CurvePoseFilter", _curvePoseFilter, sizeof(_curvePoseFilter));

    const string filter = _curvePoseFilter;
    ImGui::BeginChild("CurvePoseList", ImVec2(0.f, 160.f), true);
    for (uint32 i = 0; i < poses.size(); ++i)
    {
        const string& poseName = poses[i].name;
        if (!filter.empty() && poseName.find(filter) == string::npos)
            continue;

        const bool selected = (_selectedCurvePoseIndex == static_cast<int32>(i));
        if (ImGui::Selectable((poseName + "##CurvePose" + to_string(i)).c_str(), selected))
            _selectedCurvePoseIndex = static_cast<int32>(i);
    }
    ImGui::EndChild();
}

void ModelTestPanel::Render_SelectedCurveClipPose()
{
    const auto& poses = _curveClip->Get_Poses();
    if (_selectedCurvePoseIndex < 0 ||
        _selectedCurvePoseIndex >= static_cast<int32>(poses.size()))
    {
        return;
    }

    const CurveClip::CurveClip_Pose& pose = poses[_selectedCurvePoseIndex];
    const auto [matchedCount, curveCount] =
        Count_CurveClipTargetMatches(*_curveClip, static_cast<uint32>(_selectedCurvePoseIndex));

    ImGui::TextWrapped("Selected: %s", pose.name.c_str());
    ImGui::Text("Curves: %u / Matched Curve Targets: %u", curveCount, matchedCount);
    if (ImGui::SliderFloat("Preview Weight##CurvePoseWeight", &_curvePoseWeight, -25.f, 25.f))
    {
        if (auto modelCom = Get_CurrentModelComponent())
        {
            if (modelCom->Find_AnimModifierByTag(Get_CurveModifierTag()) != nullptr)
                Apply_CurveClipPose();
        }
    }

    if (ImGui::Button("Apply CurveClip Modifier", ImVec2(-1.f, 0.f)))
        Apply_CurveClipPose();
}

void ModelTestPanel::Disable_CurveModifier()
{
    auto modelCom = Get_CurrentModelComponent();
    if (modelCom == nullptr)
        return;

    if (auto* modifier = modelCom->Find_AnimModifierByTag(Get_CurveModifierTag()))
        modifier->Set_Active(false);

    modelCom->Reset_MorphWeights();
    _curveClipStatus = "CurveClip modifier disabled.";
}

bool ModelTestPanel::Browse_File(const wchar_t* filter, char* outBuffer, size_t bufferSize) const
{
    wchar_t filePath[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn))
        return false;

    strcpy_s(outBuffer, bufferSize, ToString(filePath).c_str());
    return true;
}

HRESULT ModelTestPanel::Load_CurveClip()
{
    const fs::path clipPath = String::ToWString(_curveClipPathBuffer);
    if (clipPath.empty() || !fs::exists(clipPath))
    {
        _curveClipStatus = "CurveClip load failed: file does not exist.";
        return E_FAIL;
    }

    auto clip = CurveClip::Create();
    if (FAILED(clip->Load_Binary(String::ToString(clipPath.wstring()))))
    {
        _curveClipStatus = "CurveClip load failed: unsupported binary.";
        return E_FAIL;
    }

    _curveClip = clip;
    _selectedCurvePoseIndex = !_curveClip->Get_Poses().empty() ? 0 : -1;
    _curveClipStatus =
        "CurveClip loaded: " + clipPath.filename().string() +
        " | poses=" + to_string(static_cast<uint32>(_curveClip->Get_Poses().size())) +
        " | press Apply to preview.";
    return S_OK;
}

HRESULT ModelTestPanel::Build_SelectedCurveClipJson()
{
    const fs::path jsonPath = String::ToWString(_curveJsonPathBuffer);
    if (jsonPath.empty() || !fs::exists(jsonPath))
    {
        _curveClipStatus = "CurveClip build failed: json file does not exist.";
        return E_FAIL;
    }

    if (!Has_FileExtension(jsonPath, POSE_ASSET_JSON_EXTENSION))
    {
        _curveClipStatus = "CurveClip build failed: selected file is not json.";
        return E_FAIL;
    }

    fs::path builtPath{};
    if (FAILED(Build_CurveClipFromJsonFile(jsonPath, &builtPath)))
    {
        _curveClipStatus = "CurveClip build failed: unsupported curve json.";
        return E_FAIL;
    }

    strcpy_s(_curveClipPathBuffer, String::ToString(builtPath.wstring()).c_str());
    CHECK_FAILED(Load_CurveClip(), E_FAIL);

    _curveClipStatus =
        "CurveClip built and loaded: " + builtPath.filename().string() +
        " | poses=" + to_string(static_cast<uint32>(_curveClip->Get_Poses().size()));
    return S_OK;
}

void ModelTestPanel::Apply_CurveClipPose()
{
    auto modelCom = Get_CurrentModelComponent();
    if (modelCom == nullptr || modelCom->Get_MorphData() == nullptr || _curveClip == nullptr)
    {
        _curveClipStatus = "CurveClip apply failed: model curve targets are not ready.";
        return;
    }

    if (_selectedCurvePoseIndex < 0 ||
        _selectedCurvePoseIndex >= static_cast<int32>(_curveClip->Get_Poses().size()))
    {
        _curveClipStatus = "CurveClip apply failed: pose is not selected.";
        return;
    }

    const wstring modifierTag = Get_CurveModifierTag();
    auto* existing = modelCom->Find_AnimModifierByTag(modifierTag);
    auto* modifier = dynamic_cast<CurveAnimationModifier*>(existing);
    if (existing != nullptr && modifier == nullptr)
    {
        _curveClipStatus = "CurveClip apply failed: modifier tag is already used.";
        return;
    }

    if (modifier == nullptr)
    {
        CurveAnimationModifier::CurveModifierDesc desc{};
        desc.modifierTag = modifierTag;
        desc.clip = _curveClip;
        desc.poseIndex = _selectedCurvePoseIndex;
        desc.poseWeight = _curvePoseWeight;
        desc.allowWeightOvershoot = true;

        auto created = CurveAnimationModifier::Create(&desc);
        if (created == nullptr)
        {
            _curveClipStatus = "CurveClip apply failed: modifier creation failed.";
            return;
        }

        modifier = created.get();
        modelCom->Add_AnimModifier(created);
    }
    else
    {
        modifier->Set_CurveClip(_curveClip);
        if (!modifier->Set_PoseIndex(_selectedCurvePoseIndex))
        {
            _curveClipStatus = "CurveClip apply failed: invalid pose index.";
            return;
        }
        modifier->Set_PoseWeight(_curvePoseWeight);
        modifier->Set_AllowWeightOvershoot(true);
        modifier->Set_Active(true);
    }

    const auto [matchedCount, curveCount] =
        Count_CurveClipTargetMatches(*_curveClip, static_cast<uint32>(_selectedCurvePoseIndex));

    modifier->Evaluate_Modifier(modelCom, 0.f);

    uint32 activeWeightCount = 0;
    float maxWeight = 0.f;
    if (const vector<float>* weights = modelCom->Get_MorphWeights())
    {
        for (float weight : *weights)
        {
            const float absWeight = std::abs(weight);
            if (absWeight <= ZERO_CURVE_EPSILON)
                continue;

            ++activeWeightCount;
            maxWeight = std::max(maxWeight, absWeight);
        }
    }

    _curveClipStatus =
        "CurveClip modifier applied: matched " + to_string(matchedCount) +
        " / " + to_string(curveCount) +
        " curves | active weights=" + to_string(activeWeightCount) +
        " | max=" + to_string(maxWeight);
}

HRESULT ModelTestPanel::Build_CurveClipsFromCurrentModelFolder()
{
    const fs::path modelDir = Get_CurrentModelDirectory();
    if (modelDir.empty() || !fs::exists(modelDir))
    {
        _curveClipStatus = "CurveClip build failed: model folder does not exist.";
        return E_FAIL;
    }

    uint32 scannedCount = 0;
    uint32 builtCount = 0;
    for (const auto& entry : fs::recursive_directory_iterator(modelDir))
    {
        if (!entry.is_regular_file() || !Has_FileExtension(entry.path(), POSE_ASSET_JSON_EXTENSION))
            continue;

        ++scannedCount;

        if (SUCCEEDED(Build_CurveClipFromJsonFile(entry.path())))
            ++builtCount;
    }

    _curveClipStatus =
        "CurveClip scan complete: built " + to_string(builtCount) +
        " / " + to_string(scannedCount) + " json files.";
    return builtCount > 0 ? S_OK : E_FAIL;
}

HRESULT ModelTestPanel::Build_CurvePresetFromCurrentModelFolder()
{
    auto modelCom = Get_CurrentModelComponent();
    if (modelCom == nullptr)
        return E_FAIL;

    const fs::path modelPath = modelCom->Get_SourceModelPath();
    const fs::path modelDir = Get_CurrentModelDirectory();
    if (modelDir.empty() || !fs::exists(modelDir))
    {
        _curveClipStatus = "CurvePreset build failed: model folder does not exist.";
        return E_FAIL;
    }

    vector<fs::path> clipPaths{};
    for (const auto& entry : fs::recursive_directory_iterator(modelDir))
    {
        if (entry.is_regular_file() && Has_FileExtension(entry.path(), CurveClipFormat::EXTENSION_W))
            clipPaths.push_back(entry.path());
    }

    ranges::sort(clipPaths, [](const fs::path& lhs, const fs::path& rhs)
    {
        return lhs.wstring() < rhs.wstring();
    });
    if (clipPaths.empty())
    {
        _curveClipStatus = "CurvePreset build failed: no curve clip binaries found.";
        return E_FAIL;
    }

    vector<string> clipPathStrings{};
    clipPathStrings.reserve(clipPaths.size());
    for (const fs::path& clipPath : clipPaths)
        clipPathStrings.push_back(String::ToString(clipPath.wstring()));

    const string modelPathString = String::ToString(modelPath.wstring());
    const string presetPath =
        CurveAnimationModifier::Build_PresetPathForModel(modelPathString, Get_CurveModifierTag());
    CHECK_FAILED(CurveAnimationModifier::Save_Preset(presetPath, clipPathStrings), E_FAIL);

    _curveClipStatus =
        "CurvePreset built: " + fs::path(String::ToWString(presetPath)).filename().string() +
        " | clips=" + to_string(static_cast<uint32>(clipPaths.size()));
    return S_OK;
}

HRESULT ModelTestPanel::Build_CurveClipFromJsonFile(const fs::path& jsonPath, fs::path* outBuiltPath) const
{
    auto clip = CurveClip::Create();
    CHECK_NULL(clip, E_FAIL);
    if (FAILED(Load_CurveJsonForBuild(jsonPath, *clip)))
        return E_FAIL;

    fs::path outputPath = jsonPath;
    outputPath.replace_extension(CurveClipFormat::EXTENSION_W);
    CHECK_FAILED(Save_CurveClipBinary(*clip, outputPath), E_FAIL);

    if (outBuiltPath != nullptr)
        *outBuiltPath = outputPath;

    return S_OK;
}

fs::path ModelTestPanel::Get_CurrentModelDirectory() const
{
    auto modelCom = Get_CurrentModelComponent();
    if (modelCom == nullptr)
        return {};

    const fs::path modelPath = modelCom->Get_SourceModelPath();
    return modelPath.has_extension() ? modelPath.parent_path() : modelPath;
}

void ModelTestPanel::Apply_CurvePresetModifier()
{
    auto modelCom = Get_CurrentModelComponent();
    if (modelCom == nullptr)
        return;

    const wstring modifierTag = Get_CurveModifierTag();

    auto* existing = modelCom->Find_AnimModifierByTag(modifierTag);
    auto* modifier = dynamic_cast<CurveAnimationModifier*>(existing);
    if (existing != nullptr && modifier == nullptr)
    {
        _curveClipStatus = "CurvePreset load failed: modifier tag is already used.";
        return;
    }

    Shared<CurveAnimationModifier> created{};
    if (modifier == nullptr)
    {
        CurveAnimationModifier::CurveModifierDesc desc{};
        desc.modifierTag = modifierTag;
        desc.poseWeight = _curvePoseWeight;
        desc.allowWeightOvershoot = true;

        created = CurveAnimationModifier::Create(&desc);
        if (created == nullptr)
        {
            _curveClipStatus = "CurvePreset load failed: modifier creation failed.";
            return;
        }

        modifier = created.get();
    }

    const fs::path modelPath = modelCom->Get_SourceModelPath();
    if (FAILED(modifier->Load_PresetForModel(String::ToString(modelPath.wstring()))))
    {
        _curveClipStatus = "CurvePreset load failed: matching preset was not found.";
        return;
    }

    modifier->Set_PoseWeight(_curvePoseWeight);
    modifier->Set_AllowWeightOvershoot(true);
    modifier->Set_Active(true);

    if (created != nullptr)
        modelCom->Add_AnimModifier(created);

    const auto& clips = modifier->Get_Clips();
    _curveClip = !clips.empty() ? clips.front() : nullptr;
    _selectedCurvePoseIndex = _curveClip != nullptr && !_curveClip->Get_Poses().empty() ? 0 : -1;

    _curveClipStatus =
        "CurvePreset loaded for modifier: " + String::ToString(modifierTag) +
        " | clips=" + to_string(static_cast<uint32>(clips.size()));
}

wstring ModelTestPanel::Get_CurveModifierTag() const
{
    if (_curveModifierNameBuffer[0] == '\0')
        return String::ToWString(DEFAULT_CURVE_MODIFIER_TAG);

    return String::ToWString(_curveModifierNameBuffer);
}

pair<uint32, uint32> ModelTestPanel::Count_CurveClipTargetMatches(
    const Engine::CurveClip& clip,
    uint32 poseIndex) const
{
    auto modelCom = Get_CurrentModelComponent();
    if (modelCom == nullptr)
        return { 0, 0 };

    const auto* pose = clip.Get_Pose(poseIndex);
    if (pose == nullptr)
        return { 0, 0 };

    uint32 matchedCount = 0;
    for (const CurveClip::Curve& curve : pose->curves)
    {
        if (modelCom->Find_MorphTargetIndex(curve.name) >= 0)
            ++matchedCount;
    }

    return { matchedCount, static_cast<uint32>(pose->curves.size()) };
}

void ModelTestPanel::Render_MotionWarpingTest()
{
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Motion Warping Test");
    ImGui::Separator();

    auto viewer = ModelImGui_Manager::GetInstance()->Get_CurrentModelObject();
    if (!viewer || !viewer->Get_ModelComponent()) {
        ImGui::TextDisabled("No model loaded.");
        return;
    }

    auto modelCom = viewer->Get_ModelComponent();
    auto transformCom = viewer->Get_Transform();

    // 1. 목표 거리 및 스피드 설정
    ImGui::DragFloat("Target Distance (m)", &_testTargetDistance, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Test Speed", &_testSpeed, 0.05f, 0.1f, 10.0f);

    // 2. 현재 상태 정보 표시
    float currentRatio = modelCom->Get_RootMotionTranslationRatio();
    ImGui::ProgressBar(currentRatio, ImVec2(0.0f, 0.0f), "Ratio");
    ImGui::Text("Current Ratio: %.3f", currentRatio);

    // 3. 테스트 실행/중지 버튼
    if (!_testWarpingActive)
    {
        if (ImGui::Button("Execute Warp Test", ImVec2(-1, 30)))
        {
            _testWarpingActive = true;
            
            // 위치 초기화 및 기준점 동기화
            transformCom->Set_Position(Vec3::Zero);
            _testStartPos = Vec3::Zero; 

            // 애니메이션 강제 리셋 및 즉시 갱신 (순간이동 방지)
            modelCom->Set_AnimationPaused(false);
            modelCom->Set_CurrentAnimationTimeSec(0.f);
            modelCom->Refresh_CurrentAnimationPose();

            // 리셋 직후의 실제 Ratio를 첫 값으로 저장
            _testPrevRatio = modelCom->Get_RootMotionTranslationRatio();
        }
    }
    else
    {
        if (ImGui::Button("Stop Test", ImVec2(-1, 30)))
        {
            _testWarpingActive = false;
            modelCom->Set_AnimationPaused(true);
        }
    }

    // 4. [핵심 로직] 워핑 실행 중일 때 좌표 이동
    if (_testWarpingActive)
    {
        float deltaRatio = currentRatio - _testPrevRatio;

        // 애니메이션이 바뀌었거나 루프되어 비율이 역전된 경우 또는 끝난 경우 처리
        if (deltaRatio < -0.05f || currentRatio >= 0.999f)
        {
            _testWarpingActive = false;
        }

        else if (deltaRatio > 0.0f)
        {
            Vec3 forward = transformCom->Get_LocalForward(); 
            forward.Normalize();
            
            Vec3 moveStep = forward * (_testTargetDistance * deltaRatio * _testSpeed);

            moveStep.z *= -1.0f;

            Vec3 currentPos = transformCom->Get_Position();
            transformCom->Set_Position(currentPos + moveStep);
        }
        
        _testPrevRatio = currentRatio;
    }

    ImGui::Separator();

    // 5. 위치 및 상태 초기화 버튼
    if (ImGui::Button("Reset Position & State", ImVec2(-1, 0)))
    {
        _testWarpingActive = false;
        _testPrevRatio = 0.0f;
        _testStartPos = Vec3::Zero;

        transformCom->Set_Position(Vec3::Zero);
        modelCom->Set_AnimationPaused(true);
        modelCom->Set_CurrentAnimationTimeSec(0.f);
        modelCom->Refresh_CurrentAnimationPose();
    }

    // 현재 위치 정보 출력
    Vec3 curPos = transformCom->Get_Position();
    ImGui::Text("Current Pos: X:%.2f, Y:%.2f, Z:%.2f", curPos.x, curPos.y, curPos.z);
    ImGui::Text("Distance from Start: %.3f m", Vec3::Distance(_testStartPos, curPos));
}

Shared<ModelTestPanel> ModelTestPanel::Create()
{
	auto instance = make_shared<ModelTestPanel>();
	if (FAILED(instance->Initialize()))
	{
		return nullptr;
	}
	return instance;
}

NS_END
