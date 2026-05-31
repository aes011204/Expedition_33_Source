#pragma once
#include "Client_Defines.h"
#include "EffectEmitter.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
NS_END

NS_BEGIN(Client)

class MeshEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(MeshEmitter, ObjectType::EffectEmitter);

public:
    MeshEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MeshEmitter(const MeshEmitter& prototype);
    ~MeshEmitter() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    HRESULT Reset_ForEffectReplay() override;
    bool Is_EffectFinished() const override;

protected:
    void On_EffectTransformSynced() override;

private:
    static constexpr auto kMeshShaderPath{ "../../../Client/Bin/ShaderFiles/Shader_EffectMesh.hlsl" };

private:
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct MeshInstanceVertex
    {
        Matrix world{};
        Vec4 color{ 1.f, 1.f, 1.f, 1.f };
        Vec2 lifeTime{};
        Vec2 padding{};
    };

    struct MeshParticleState
    {
        bool active{ false };
        float age{};
        float lifeMax{ 1.f };
        Vec3 startPosition{};
        Vec3 velocity{};
        Vec3 baseScale{ 1.f, 1.f, 1.f };
        float initialRollRadians{};
        float rollRateRadians{};
        Vec4 startColor{ 1.f, 1.f, 1.f, 1.f };
        Vec4 endColor{ 1.f, 1.f, 1.f, 1.f };
        uint32 seed{};
    };

private:
    MeshEmitterDesc _desc{};
    Shared<ModelCom> _model{};
    Shared<ShaderCom> _shader{};
    ComPtr<Buffer> _instanceBuffer{};
    vector<MeshParticleState> _particles{};
    vector<MeshInstanceVertex> _instancePayload{};
    vector<bool> _burstExecuted{};
    float _emitterElapsedTime{};
    float _spawnAccumulator{};
    PlaybackState _playbackState{ PlaybackState::Delayed };
    float _loopElapsedTime{};
    uint32 _loopIndex{};
    bool _completedKillDispatched{};
    uint32 _activeInstanceCount{};
    uint32 _nextSpawnCursor{};
    bool _isReady{ false };

private:
    HRESULT Ready_Components();
    HRESULT Ready_Shader();
    HRESULT Ready_InstanceBuffer();
    HRESULT Apply_EffectMaterialOverride();
    void Reset_PlaybackRuntime();
    void Advance_Playback(float timeDelta);
    void Start_NextLoop();
    float Resolve_CurrentLoopDelay() const;
    float Resolve_Duration() const;
    bool Has_NextLoop() const;
    bool Can_SubmitRender() const;
    uint32 Consume_SpawnRequest(float timeDelta);
    void Spawn_Particles(uint32 spawnRequest);
    void Activate_Particle(MeshParticleState& particle, uint32 seed);
    void Update_Particles(float timeDelta);
    void Update_InstanceBuffer();
    wstring Resolve_ModelPath() const;
    wstring Resolve_MaterialTexturePath(const string& textureGuid, const string& texturePath) const;
    wstring Resolve_ShaderPath() const;
    Matrix Build_InstanceMatrix(const MeshParticleState& particle) const;
    Vec3 Sample_InitialOffset(uint32 seed) const;
    Vec3 Sample_InitialVelocity(const Vec3& spawnPosition, uint32 seed) const;
    Vec4 Sample_Color(const Vec4& minColor, const Vec4& maxColor, uint32 seed) const;
    float Sample_InitialScale(uint32 seed) const;
    float Evaluate_SizeByLife(float lifeProgress) const;
    Vec4 Evaluate_ColorOverLife(const MeshParticleState& particle, float lifeProgress) const;
    Vec3 Resolve_LocalVector(const Vec3& localVector) const;
    float Random01(uint32 seed) const;
    float RandomRange(float minValue, float maxValue, uint32 seed) const;

public:
    static Shared<MeshEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
