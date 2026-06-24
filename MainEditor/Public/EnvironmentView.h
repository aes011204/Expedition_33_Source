#pragma once

#include "Editor_Window.h"
#include "imgui.h"
#include "ImGuizmo.h"

NS_BEGIN(MainEditor)

class EnvironmentView final : public Editor_Window
{
public:
    enum class EntryKind : uint8
    {
        None,
        Fog,
        Froxel
    };

    enum class ZoneShape : uint8
    {
        Box,
        OBB,
        Sphere
    };

    struct VolumeTransform
    {
        string name = {};
        ZoneShape shape = ZoneShape::Box;
        Vec3 position = Vec3::Zero;
        Quat rotation = Quat::Identity;
        Vec3 extent = Vec3(5.f, 5.f, 5.f);
        float radius = 5.f;
    };

    struct FogEntry
    {
        VolumeTransform volume = {};
        Vec3 fogColor = Vec3(1.f, 1.f, 1.f);
        float fogDensity = 0.f;
        float baseHeight = 0.f;
        float topFadeStart = 0.4f;
        float topHeight = 0.9f;
        float edgeFade = 1.f;
        float noiseScale = 0.0025f;
        Vec2 noiseSpeed = Vec2(0.005f, 0.003f);
        float noiseStrength = 0.9f;
        float noiseMin = 0.38f;
        float groundFogMaxDistance = 100.f;
        float groundFogMinAccumulationDistance = 10.f;
        float groundFogDistanceInfluence = 0.15f;
        float groundFogNearFadeDistance = 12.f;
        float groundFogNearMinFactor = 0.65f;
    };

    struct FroxelEntry
    {
        VolumeTransform volume = {};
        float density = 1.f;
        Vec4 lightColor = Vec4(1.f, 0.95f, 0.8f, 1.f);
        float edgeFade = 0.5f;
        float intensity = 1.f;
    };

public:
    EnvironmentView();
    ~EnvironmentView() override = default;

public:
    void Update(float timeDelta) override;
    void Render() override;

public:
    static Shared<EnvironmentView> Create();

    static void ClearSelection();
    static bool HasSelectedEntry();
    static void DrawDebugVolumes();
    static bool ManipulateSelectedEntry(
        const Matrix* viewMatrix,
        const Matrix* projMatrix,
        const ImVec2& viewportMin,
        const ImVec2& viewportMax,
        ImGuizmo::OPERATION operation,
        ImGuizmo::MODE mode);

private:
    static void RefreshFromRuntimeEnvironment();
    static void EnsureRuntimeEnvironmentImported();
    static void ApplyRuntimeEnvironment();
    static void SelectEntry(EntryKind kind, int32 index);
    static void DeleteSelectedEntry();
    static void DuplicateSelectedEntry();
    static void AddFogEntry();
    static void AddFroxelEntry();
    static Vec3 ResolveExtent(const VolumeTransform& volume);
    static Matrix BuildVolumeWorldMatrix(const VolumeTransform& volume);
    static bool IsFogOBBShape(ZoneShape shape) { return shape == ZoneShape::OBB; }
    static int32 GetFogRuntimeIndex(int32 editorIndex, ZoneShape shape);
    static const FogEntry* FindFogEntryByRuntimeIndex(ZoneShape shape, uint32 runtimeIndex);

    void RenderFogSection();
    void RenderFroxelSection();
    void RenderCullingSection();
    void RenderSelectedInspector();
    void RenderVolumeTransformEditor(VolumeTransform& volume);
    void RenderFogVolumeTransformEditor(FogEntry& entry);
    void RenderFogEditor(FogEntry& entry);
    void RenderFroxelEditor(FroxelEntry& entry);

private:
    inline static vector<FogEntry> _fogEntries{};
    inline static vector<FroxelEntry> _froxelEntries{};
    inline static EntryKind _selectedKind = EntryKind::None;
    inline static int32 _selectedIndex = -1;
    inline static bool _runtimeImported = false;
    inline static bool _runtimeDirty = false;
};

NS_END
