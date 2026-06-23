#include "pch.h"
#include "EnvironmentView.h"

#include "EditorInstance.h"
#include "GameInstance.h"
#include "Shader_CBuffer_Define.h"

namespace MainEditor
{
    namespace
    {
        Matrix BuildWorldToLocal(const EnvironmentView::VolumeTransform& volume)
        {
            const Matrix world =
                Matrix::CreateFromQuaternion(volume.rotation) *
                Matrix::CreateTranslation(volume.position);
            return world.Invert();
        }

        bool ContainsIndex(const vector<uint32>& indices, uint32 index)
        {
            return find(indices.begin(), indices.end(), index) != indices.end();
        }

        const char* ResolveFogEntryName(uint32 index, const vector<EnvironmentView::FogEntry>& entries)
        {
            if (index < entries.size())
                return entries[index].volume.name.c_str();

            return "Runtime Fog Zone";
        }

        const char* ResolveFroxelEntryName(uint32 index, const vector<EnvironmentView::FroxelEntry>& entries)
        {
            if (index < entries.size())
                return entries[index].volume.name.c_str();

            return "Runtime Froxel Shaft";
        }
    }

    EnvironmentView::EnvironmentView()
        : Editor_Window{ L"Environment" }
    {
    }

    void EnvironmentView::Update(float timeDelta)
    {
        Editor_Window::Update(timeDelta);
        UNREFERENCED_PARAMETER(timeDelta);

        if (_runtimeDirty)
            ApplyRuntimeEnvironment();
    }

    void EnvironmentView::Render()
    {
        if (!Is_Open())
            return;

        EnsureRuntimeEnvironmentImported();

        bool isOpen = Is_Open();
        const string windowName = String::ToString(Get_WindowName());

        if (ImGui::Begin(windowName.c_str(), &isOpen))
        {
            if (ImGui::Button("Add Fog"))
                AddFogEntry();

            ImGui::SameLine();
            if (ImGui::Button("Add Froxel Shaft"))
                AddFroxelEntry();

            ImGui::SameLine();
            if (ImGui::Button("Duplicate"))
                DuplicateSelectedEntry();

            ImGui::SameLine();
            if (ImGui::Button("Delete"))
                DeleteSelectedEntry();

            ImGui::SameLine();
            if (ImGui::Button("Refresh From Runtime"))
                RefreshFromRuntimeEnvironment();

            ImGui::Separator();
            RenderCullingSection();
            ImGui::Separator();
            RenderFogSection();
            ImGui::Separator();
            RenderFroxelSection();
            ImGui::Separator();
            RenderSelectedInspector();
        }

        Set_Open(isOpen);
        ImGui::End();
    }

    Shared<EnvironmentView> EnvironmentView::Create()
    {
        return make_shared<EnvironmentView>();
    }

    void EnvironmentView::ClearSelection()
    {
        _selectedKind = EntryKind::None;
        _selectedIndex = -1;
    }

    bool EnvironmentView::HasSelectedEntry()
    {
        if (_selectedIndex < 0)
            return false;

        if (_selectedKind == EntryKind::Fog)
            return _selectedIndex < static_cast<int32>(_fogEntries.size());

        if (_selectedKind == EntryKind::Froxel)
            return _selectedIndex < static_cast<int32>(_froxelEntries.size());

        return false;
    }

    void EnvironmentView::DrawDebugVolumes()
    {
        auto drawVolume = [](const VolumeTransform& volume, bool selected, const Color& color)
        {
            const Color drawColor = selected ? Color(1.f, 1.f, 0.f, 1.f) : color;

            if (volume.shape == ZoneShape::Sphere)
            {
                DebugSphereDesc desc{};
                desc.center = volume.position;
                desc.radius = volume.radius;
                desc.style.color = drawColor;
                desc.style.depthEnabled = false;
                GAME->Draw_DebugSphere(desc);
                return;
            }

            DebugBoxDesc desc{};
            desc.center = volume.position;
            desc.extents = volume.extent;
            desc.rotation = volume.rotation;
            desc.style.color = drawColor;
            desc.style.depthEnabled = false;
            GAME->Draw_DebugBox(desc);
        };

        for (int32 i = 0; i < static_cast<int32>(_fogEntries.size()); ++i)
            drawVolume(_fogEntries[i].volume, _selectedKind == EntryKind::Fog && _selectedIndex == i, Color(0.35f, 0.8f, 1.f, 1.f));

        for (int32 i = 0; i < static_cast<int32>(_froxelEntries.size()); ++i)
            drawVolume(_froxelEntries[i].volume, _selectedKind == EntryKind::Froxel && _selectedIndex == i, Color(1.f, 0.85f, 0.3f, 1.f));
    }

    bool EnvironmentView::ManipulateSelectedEntry(
        const Matrix* viewMatrix,
        const Matrix* projMatrix,
        const ImVec2& viewportMin,
        const ImVec2& viewportMax,
        ImGuizmo::OPERATION operation,
        ImGuizmo::MODE mode)
    {
        if (!HasSelectedEntry() || viewMatrix == nullptr || projMatrix == nullptr)
            return false;

        VolumeTransform* volume = nullptr;
        if (_selectedKind == EntryKind::Fog)
            volume = &_fogEntries[_selectedIndex].volume;
        else if (_selectedKind == EntryKind::Froxel)
            volume = &_froxelEntries[_selectedIndex].volume;

        if (volume == nullptr)
            return false;

        const float viewportWidth = viewportMax.x - viewportMin.x;
        const float viewportHeight = viewportMax.y - viewportMin.y;
        if (viewportWidth <= 1.f || viewportHeight <= 1.f)
            return false;

        Matrix worldMatrix = BuildVolumeWorldMatrix(*volume);

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(viewportMin.x, viewportMin.y, viewportWidth, viewportHeight);

        const bool manipulated = ImGuizmo::Manipulate(
            reinterpret_cast<float*>(const_cast<Matrix*>(viewMatrix)),
            reinterpret_cast<float*>(const_cast<Matrix*>(projMatrix)),
            operation,
            mode,
            reinterpret_cast<float*>(&worldMatrix));

        if (!manipulated)
            return false;

        Vec3 scale = Vec3::One;
        Quat rotation = Quat::Identity;
        Vec3 translation = Vec3::Zero;
        if (!worldMatrix.Decompose(scale, rotation, translation))
            return false;

        volume->position = translation;
        volume->rotation = rotation;
        if (volume->shape == ZoneShape::Sphere)
        {
            volume->radius = max(abs(scale.x), max(abs(scale.y), abs(scale.z)));
        }
        else
        {
            volume->extent = Vec3(abs(scale.x), abs(scale.y), abs(scale.z));
        }

        _runtimeDirty = true;
        ApplyRuntimeEnvironment();
        return true;
    }

    void EnvironmentView::RefreshFromRuntimeEnvironment()
    {
        _fogEntries.clear();
        _froxelEntries.clear();
        ClearSelection();

        const FogZoneCB& fogCB = GAME->Get_FogZones();
        for (int32 i = 0; i < fogCB.aabbCount; ++i)
        {
            const FogZoneEntry& zone = fogCB.zones[i];

            FogEntry entry{};
            entry.volume.name = "FogZone_" + to_string(i);
            entry.volume.shape = ZoneShape::Box;
            entry.volume.position = (zone.zoneMin + zone.zoneMax) * 0.5f;
            entry.volume.extent = (zone.zoneMax - zone.zoneMin) * 0.5f;
            entry.fogColor = Vec3(zone.fogColor.x, zone.fogColor.y, zone.fogColor.z);
            entry.fogDensity = zone.fogDensity;
            entry.baseHeight = zone.baseHeight;
            entry.topFadeStart = zone.topFadeStart;
            entry.topHeight = zone.topHeight;
            entry.edgeFade = zone.edgeFade;
            entry.noiseScale = zone.noiseScale;
            entry.noiseSpeed = zone.noiseSpeed;
            entry.noiseStrength = zone.noiseStrength;
            entry.noiseMin = zone.noiseMin;
            entry.groundFogMaxDistance = fogCB.groundFogMaxDistance;
            entry.groundFogMinAccumulationDistance = fogCB.groundFogMinAccumulationDistance;
            entry.groundFogDistanceInfluence = fogCB.groundFogDistanceInfluence;
            entry.groundFogNearFadeDistance = fogCB.groundFogNearFadeDistance;
            entry.groundFogNearMinFactor = fogCB.groundFogNearMinFactor;
            _fogEntries.push_back(entry);
        }

        const VolumetricLightShaftZoneCB& shaftCB = GAME->Get_VolumetricLightShaftZones();
        for (uint32 i = 0; i < shaftCB.zoneCount; ++i)
        {
            const VolumetricLightShaftZoneEntry& zone = shaftCB.zones[i];

            FroxelEntry entry{};
            entry.volume.name = "FroxelShaft_" + to_string(i);
            entry.volume.shape = ZoneShape::Box;

            Matrix localToWorld = zone.worldToLocal.Invert();
            Vec3 scale = Vec3::One;
            Quat rotation = Quat::Identity;
            Vec3 translation = Vec3::Zero;
            if (localToWorld.Decompose(scale, rotation, translation))
            {
                entry.volume.position = translation;
                entry.volume.rotation = rotation;
            }

            entry.volume.extent = zone.halfExtents;
            entry.density = zone.density;
            entry.lightColor = zone.lightColor;
            entry.edgeFade = zone.edgeFade;
            entry.intensity = zone.intensity;
            _froxelEntries.push_back(entry);
        }

        _runtimeImported = true;
        _runtimeDirty = false;
    }

    void EnvironmentView::EnsureRuntimeEnvironmentImported()
    {
        const FogZoneCB& fogCB = GAME->Get_FogZones();
        const VolumetricLightShaftZoneCB& shaftCB = GAME->Get_VolumetricLightShaftZones();

        const bool hasRuntimeEnvironment =
            (fogCB.aabbCount > 0 || fogCB.sphereCount > 0 || shaftCB.zoneCount > 0);

        if (!_runtimeImported)
        {
            if (hasRuntimeEnvironment)
                RefreshFromRuntimeEnvironment();
            return;
        }

        if (_runtimeDirty)
            return;

        if (_fogEntries.empty() && _froxelEntries.empty() && hasRuntimeEnvironment)
        {
            RefreshFromRuntimeEnvironment();
            return;
        }

        if (static_cast<int32>(_fogEntries.size()) != fogCB.aabbCount ||
            static_cast<uint32>(_froxelEntries.size()) != shaftCB.zoneCount)
        {
            RefreshFromRuntimeEnvironment();
        }
    }

    void EnvironmentView::ApplyRuntimeEnvironment()
    {
        FogZoneCB fogCB{};
        bool hasGroundFogGlobals = false;

        for (const FogEntry& entry : _fogEntries)
        {
            if (entry.fogDensity <= 0.f)
                continue;

            if (!hasGroundFogGlobals)
            {
                fogCB.groundFogMaxDistance = entry.groundFogMaxDistance;
                fogCB.groundFogMinAccumulationDistance = entry.groundFogMinAccumulationDistance;
                fogCB.groundFogDistanceInfluence = entry.groundFogDistanceInfluence;
                fogCB.groundFogNearFadeDistance = entry.groundFogNearFadeDistance;
                fogCB.groundFogNearMinFactor = entry.groundFogNearMinFactor;
                hasGroundFogGlobals = true;
            }

            if (fogCB.aabbCount >= FogZoneCB::MAX_FOG_ZONES)
                break;

            FogZoneEntry zone{};
            const Vec3 extent = ResolveExtent(entry.volume);
            zone.zoneMin = entry.volume.position - extent;
            zone.zoneMax = entry.volume.position + extent;
            zone.baseHeight = entry.baseHeight;
            zone.topFadeStart = entry.topFadeStart;
            zone.fogColor = Vec4(entry.fogColor.x, entry.fogColor.y, entry.fogColor.z, 1.f);
            zone.fogDensity = entry.fogDensity;
            zone.topHeight = entry.topHeight;
            zone.edgeFade = entry.edgeFade;
            zone.noiseScale = entry.noiseScale;
            zone.noiseSpeed = entry.noiseSpeed;
            zone.noiseStrength = entry.noiseStrength;
            zone.noiseMin = entry.noiseMin;
            fogCB.zones[fogCB.aabbCount++] = zone;
        }

        VolumetricLightShaftZoneCB shaftCB{};
        for (const FroxelEntry& entry : _froxelEntries)
        {
            if (shaftCB.zoneCount >= VolumetricLightShaftZoneCB::MAX_LIGHT_SHAFT_ZONES)
                break;

            VolumetricLightShaftZoneEntry zone{};
            zone.worldToLocal = BuildWorldToLocal(entry.volume);
            zone.halfExtents = ResolveExtent(entry.volume);
            zone.density = entry.density;
            zone.lightColor = entry.lightColor;
            zone.edgeFade = entry.edgeFade;
            zone.intensity = entry.intensity;
            shaftCB.zones[shaftCB.zoneCount++] = zone;
        }

        GAME->Set_FogZones(fogCB);
        GAME->Set_VolumetricLightShaftZones(shaftCB);
        _runtimeDirty = false;
    }

    void EnvironmentView::SelectEntry(EntryKind kind, int32 index)
    {
        _selectedKind = kind;
        _selectedIndex = index;
        EDITOR->Clear_SelectedRuntimeWorldUI();
        EDITOR->Clear_SelectedUI();
        EDITOR->Clear_SelectedObject();
    }

    void EnvironmentView::DeleteSelectedEntry()
    {
        if (!HasSelectedEntry())
            return;

        if (_selectedKind == EntryKind::Fog)
            _fogEntries.erase(_fogEntries.begin() + _selectedIndex);
        else if (_selectedKind == EntryKind::Froxel)
            _froxelEntries.erase(_froxelEntries.begin() + _selectedIndex);

        ClearSelection();
        _runtimeDirty = true;
    }

    void EnvironmentView::DuplicateSelectedEntry()
    {
        if (!HasSelectedEntry())
            return;

        if (_selectedKind == EntryKind::Fog)
        {
            FogEntry copy = _fogEntries[_selectedIndex];
            copy.volume.name += "_Copy";
            copy.volume.position += Vec3(1.f, 0.f, 1.f);
            _fogEntries.push_back(copy);
            SelectEntry(EntryKind::Fog, static_cast<int32>(_fogEntries.size() - 1));
        }
        else if (_selectedKind == EntryKind::Froxel)
        {
            FroxelEntry copy = _froxelEntries[_selectedIndex];
            copy.volume.name += "_Copy";
            copy.volume.position += Vec3(1.f, 0.f, 1.f);
            _froxelEntries.push_back(copy);
            SelectEntry(EntryKind::Froxel, static_cast<int32>(_froxelEntries.size() - 1));
        }

        _runtimeDirty = true;
    }

    void EnvironmentView::AddFogEntry()
    {
        EnsureRuntimeEnvironmentImported();

        FogEntry entry{};
        entry.volume.name = "FogZone_" + to_string(_fogEntries.size());
        _fogEntries.push_back(entry);
        SelectEntry(EntryKind::Fog, static_cast<int32>(_fogEntries.size() - 1));
        _runtimeDirty = true;
    }

    void EnvironmentView::AddFroxelEntry()
    {
        EnsureRuntimeEnvironmentImported();

        FroxelEntry entry{};
        entry.volume.name = "FroxelShaft_" + to_string(_froxelEntries.size());
        _froxelEntries.push_back(entry);
        SelectEntry(EntryKind::Froxel, static_cast<int32>(_froxelEntries.size() - 1));
        _runtimeDirty = true;
    }

    Vec3 EnvironmentView::ResolveExtent(const VolumeTransform& volume)
    {
        return volume.shape == ZoneShape::Sphere
            ? Vec3(volume.radius, volume.radius, volume.radius)
            : volume.extent;
    }

    Matrix EnvironmentView::BuildVolumeWorldMatrix(const VolumeTransform& volume)
    {
        const Vec3 scale = ResolveExtent(volume);
        return Matrix::CreateScale(scale) *
            Matrix::CreateFromQuaternion(volume.rotation) *
            Matrix::CreateTranslation(volume.position);
    }

    void EnvironmentView::RenderFogSection()
    {
        if (!ImGui::CollapsingHeader("Fog Entries", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        const vector<uint32>& activeIndices = GAME->Get_ActiveFogZoneIndices();
        for (int32 i = 0; i < static_cast<int32>(_fogEntries.size()); ++i)
        {
            const bool selected = (_selectedKind == EntryKind::Fog && _selectedIndex == i);
            const bool active = ContainsIndex(activeIndices, static_cast<uint32>(i));
            const string label = _fogEntries[i].volume.name + (active ? " (active)##Fog" : "##Fog");
            if (ImGui::Selectable(label.c_str(), selected))
                SelectEntry(EntryKind::Fog, i);
        }
    }

    void EnvironmentView::RenderFroxelSection()
    {
        if (!ImGui::CollapsingHeader("Froxel Shaft Entries", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        const vector<uint32>& activeIndices = GAME->Get_ActiveVolumetricLightShaftZoneIndices();
        for (int32 i = 0; i < static_cast<int32>(_froxelEntries.size()); ++i)
        {
            const bool selected = (_selectedKind == EntryKind::Froxel && _selectedIndex == i);
            const bool active = ContainsIndex(activeIndices, static_cast<uint32>(i));
            const string label = _froxelEntries[i].volume.name + (active ? " (active)##Froxel" : "##Froxel");
            if (ImGui::Selectable(label.c_str(), selected))
                SelectEntry(EntryKind::Froxel, i);
        }
    }

    void EnvironmentView::RenderCullingSection()
    {
        if (!ImGui::CollapsingHeader("Zone Culling", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        bool fogCullingEnabled = GAME->Is_FogZoneCullingEnabled();
        if (ImGui::Checkbox("Fog Culling", &fogCullingEnabled))
            GAME->Set_FogZoneCullingEnabled(fogCullingEnabled);

        float fogDistance = GAME->Get_FogZoneCullingDistance();
        if (ImGui::DragFloat("Fog Culling Distance", &fogDistance, 1.f, 0.f, 100000.f))
            GAME->Set_FogZoneCullingDistance(fogDistance);

        const vector<uint32>& activeFogIndices = GAME->Get_ActiveFogZoneIndices();
        ImGui::Text("Active Fog: %d / %d", static_cast<int32>(activeFogIndices.size()), static_cast<int32>(_fogEntries.size()));
        if (ImGui::BeginChild("ActiveFogZones", ImVec2(0.f, 72.f), true))
        {
            for (uint32 index : activeFogIndices)
                ImGui::BulletText("%u: %s", index, ResolveFogEntryName(index, _fogEntries));
        }
        ImGui::EndChild();

        bool shaftCullingEnabled = GAME->Is_VolumetricLightShaftZoneCullingEnabled();
        if (ImGui::Checkbox("Light Shaft Culling", &shaftCullingEnabled))
            GAME->Set_VolumetricLightShaftZoneCullingEnabled(shaftCullingEnabled);

        float shaftDistance = GAME->Get_VolumetricLightShaftZoneCullingDistance();
        if (ImGui::DragFloat("Light Shaft Culling Distance", &shaftDistance, 1.f, 0.f, 100000.f))
            GAME->Set_VolumetricLightShaftZoneCullingDistance(shaftDistance);

        const vector<uint32>& activeShaftIndices = GAME->Get_ActiveVolumetricLightShaftZoneIndices();
        ImGui::Text("Active Light Shaft: %d / %d", static_cast<int32>(activeShaftIndices.size()), static_cast<int32>(_froxelEntries.size()));
        if (ImGui::BeginChild("ActiveLightShaftZones", ImVec2(0.f, 72.f), true))
        {
            for (uint32 index : activeShaftIndices)
                ImGui::BulletText("%u: %s", index, ResolveFroxelEntryName(index, _froxelEntries));
        }
        ImGui::EndChild();
    }

    void EnvironmentView::RenderSelectedInspector()
    {
        if (!HasSelectedEntry())
        {
            ImGui::TextDisabled("No environment entry selected.");
            return;
        }

        if (_selectedKind == EntryKind::Fog)
            RenderFogEditor(_fogEntries[_selectedIndex]);
        else if (_selectedKind == EntryKind::Froxel)
            RenderFroxelEditor(_froxelEntries[_selectedIndex]);
    }

    void EnvironmentView::RenderVolumeTransformEditor(VolumeTransform& volume)
    {
        char nameBuffer[128] = {};
        strncpy_s(nameBuffer, volume.name.c_str(), _TRUNCATE);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            volume.name = nameBuffer;
            _runtimeDirty = true;
        }

        int shapeIndex = volume.shape == ZoneShape::Sphere ? 1 : 0;
        if (ImGui::Combo("Shape", &shapeIndex, "Box\0Sphere\0"))
        {
            volume.shape = shapeIndex == 1 ? ZoneShape::Sphere : ZoneShape::Box;
            _runtimeDirty = true;
        }

        if (ImGui::DragFloat3("Position", &volume.position.x, 0.1f))
            _runtimeDirty = true;

        Vec3 eulerDeg = volume.rotation.ToEuler() * (180.f / XM_PI);
        if (ImGui::DragFloat3("Rotation", &eulerDeg.x, 0.5f))
        {
            const Vec3 eulerRad = eulerDeg * (XM_PI / 180.f);
            volume.rotation = Quat::CreateFromYawPitchRoll(eulerRad.y, eulerRad.x, eulerRad.z);
            _runtimeDirty = true;
        }

        if (volume.shape == ZoneShape::Sphere)
        {
            if (ImGui::DragFloat("Radius", &volume.radius, 0.1f, 0.01f, 10000.f))
                _runtimeDirty = true;
        }
        else
        {
            if (ImGui::DragFloat3("Extent", &volume.extent.x, 0.1f, 0.01f, 10000.f))
                _runtimeDirty = true;
        }
    }

    void EnvironmentView::RenderFogEditor(FogEntry& entry)
    {
        ImGui::Text("Fog");
        RenderVolumeTransformEditor(entry.volume);
        if (ImGui::ColorEdit3("Fog Color", &entry.fogColor.x))
            _runtimeDirty = true;
        _runtimeDirty |= ImGui::DragFloat("Fog Density", &entry.fogDensity, 0.001f, 0.f, 1.f);
        _runtimeDirty |= ImGui::DragFloat("Base Height", &entry.baseHeight, 0.01f, 0.f, 10000.f);
        _runtimeDirty |= ImGui::DragFloat("Top Fade Start", &entry.topFadeStart, 0.01f, 0.f, 10000.f);
        _runtimeDirty |= ImGui::DragFloat("Top Height", &entry.topHeight, 0.01f, 0.f, 10000.f);
        _runtimeDirty |= ImGui::DragFloat("Edge Fade", &entry.edgeFade, 0.01f, 0.f, 10000.f);
        _runtimeDirty |= ImGui::DragFloat("Noise Scale", &entry.noiseScale, 0.0001f, 0.f, 10.f, "%.4f");
        _runtimeDirty |= ImGui::DragFloat2("Noise Speed", &entry.noiseSpeed.x, 0.0001f, -10.f, 10.f, "%.4f");
        _runtimeDirty |= ImGui::DragFloat("Noise Strength", &entry.noiseStrength, 0.01f, 0.f, 1.f);
        _runtimeDirty |= ImGui::DragFloat("Noise Min", &entry.noiseMin, 0.01f, 0.f, 1.f);

        ImGui::SeparatorText("Ground Fog Globals");
        _runtimeDirty |= ImGui::DragFloat("Max Distance", &entry.groundFogMaxDistance, 0.1f, 0.f, 10000.f);
        _runtimeDirty |= ImGui::DragFloat("Min Accum Distance", &entry.groundFogMinAccumulationDistance, 0.1f, 0.f, 10000.f);
        _runtimeDirty |= ImGui::DragFloat("Distance Influence", &entry.groundFogDistanceInfluence, 0.01f, 0.f, 1.f);
        _runtimeDirty |= ImGui::DragFloat("Near Fade Distance", &entry.groundFogNearFadeDistance, 0.1f, 0.f, 10000.f);
        _runtimeDirty |= ImGui::DragFloat("Near Min Factor", &entry.groundFogNearMinFactor, 0.01f, 0.f, 1.f);
    }

    void EnvironmentView::RenderFroxelEditor(FroxelEntry& entry)
    {
        ImGui::Text("Froxel Light Shaft");
        RenderVolumeTransformEditor(entry.volume);
        _runtimeDirty |= ImGui::DragFloat("Density", &entry.density, 0.01f, 0.f, 10.f);
        if (ImGui::ColorEdit4("Light Color", &entry.lightColor.x))
            _runtimeDirty = true;
        _runtimeDirty |= ImGui::DragFloat("Edge Fade", &entry.edgeFade, 0.01f, 0.f, 1000.f);
        _runtimeDirty |= ImGui::DragFloat("Intensity", &entry.intensity, 0.01f, 0.f, 100.f);
    }
}
