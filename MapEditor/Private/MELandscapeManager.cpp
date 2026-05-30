#include "MELandscapeManager.h"
#include "MELandscapeActor.h"
#include "MECamera.h"
#include "NoiseUtil.h"

HRESULT MELandscapeManager::Initialize(const LandMG_Desc& desc)
{
    _device = desc.device;
    _context = desc.context;
    _scene = desc.scene;
    _camera = desc.camera;
    _sculptState = desc.sculptState;
    _resourceRoot = desc.resourceRoot;
    return S_OK;
}

void MELandscapeManager::Add_Landscape(const LandscapeConfig& cfg)
{
    VIBuffer_Landscape::LAND_DESC desc{};
    desc.countX = cfg.countX;
    desc.countZ = cfg.countZ;
    desc.sizeX = cfg.sizeX;
    desc.sizeZ = cfg.sizeZ;

    auto actor = MELandscapeActor::Create(_device, _context, desc);
    if (!actor)
    {
        LOG_WARN("Add_Landscape: Failed to create MELandscapeActor");
        return;
    }

    actor->Get_Transform()->Set_Position(Vec3(cfg.worldOffsetX, cfg.worldOffsetY, cfg.worldOffsetZ));

    if (!cfg.heights.empty())
    {
        auto& heights = actor->Get_VIBuffer()->Get_Heights();
        heights = cfg.heights;
        actor->Get_VIBuffer()->Flush();
    }

    _scene->_landscapes.push_back(actor);
    _scene->_activeLandscapeIdx = static_cast<int32>(_scene->_landscapes.size()) - 1;
}

void MELandscapeManager::Remove_Landscape(int32 idx)
{
    if (idx < 0 || idx >= static_cast<int32>(_scene->_landscapes.size()))
    {
        return;
    }

    _scene->_landscapes.erase(_scene->_landscapes.begin() + idx);

    if (_scene->_activeLandscapeIdx >= static_cast<int32>(_scene->_landscapes.size()))
    {
        _scene->_activeLandscapeIdx = static_cast<int32>(_scene->_landscapes.size());
    }
}

void MELandscapeManager::Save_Landscape(int32 idx, const WCHAR* path)
{
    if (idx < 0 || idx >= static_cast<int32>(_scene->_landscapes.size()))
    {
        return;
    }

    auto vb = _scene->_landscapes[idx]->Get_VIBuffer();
    const auto& heights = vb->Get_Heights();

    FILE* f = {};
    if (_wfopen_s(&f, path, L"wb") != 0 || !f)
    {
        return;
    }

    uint32 countX = vb->Get_CountX();
    uint32 countZ = vb->Get_CountZ();
    float sizeX = vb->Get_SizeX();
    float sizeZ = vb->Get_SizeZ();

    fwrite(&countX, sizeof(uint32), 1, f);
    fwrite(&countZ, sizeof(uint32), 1, f);
    fwrite(&sizeX, sizeof(float), 1, f);
    fwrite(&sizeZ, sizeof(float), 1, f);

    Vec3 pos = _scene->_landscapes[idx]->Get_Transform()->Get_Position();
    fwrite(&pos, sizeof(Vec3), 1, f);
    fwrite(heights.data(), sizeof(float), heights.size(), f);

    uint32 layerCount = _scene->_landscapes[idx]->Get_LayerCount();
    fwrite(&layerCount, sizeof(uint32), 1, f);

    for (uint32 i = 0; i < layerCount; ++i)
    {
        const wstring& name = _scene->_landscapes[idx]->Get_LayerPath(i);
        uint32 nameLen = static_cast<uint32>(name.size());
        fwrite(&nameLen, sizeof(uint32), 1, f);
        fwrite(name.c_str(), sizeof(wchar_t), nameLen, f);

        const auto& weights = vb->Get_WeightMap(i);
        fwrite(weights.data(), sizeof(float), weights.size(), f);
    }

    fclose(f);
}

void MELandscapeManager::Load_Landscape(int32 idx, const WCHAR* path)
{
    FILE* f = {};
    if (_wfopen_s(&f, path, L"rb") != 0 || !f)
    {
        return;
    }

    uint32 countX = {}, countZ = {};
    float sizeX = {}, sizeZ = {};

    fread(&countX, sizeof(uint32), 1, f);
    fread(&countZ, sizeof(uint32), 1, f);
    fread(&sizeX, sizeof(float), 1, f);
    fread(&sizeZ, sizeof(float), 1, f);

    Vec3 pos{};
    fread(&pos, sizeof(Vec3), 1, f);

    if (idx < 0 || idx >= static_cast<int32>(_scene->_landscapes.size()))
    {
        LandscapeConfig cfg{};
        cfg.countX = countX;
        cfg.countZ = countZ;
        cfg.sizeX = sizeX;
        cfg.sizeZ = sizeZ;
        cfg.worldOffsetX = pos.x;
        cfg.worldOffsetY = pos.y;
        cfg.worldOffsetZ = pos.z;
        Add_Landscape(cfg);
        idx = static_cast<int32>(_scene->_landscapes.size()) - 1;
    }

    auto vb = _scene->_landscapes[idx]->Get_VIBuffer();
    _scene->_landscapes[idx]->Get_Transform()->Set_Position(pos);
    auto& heights = vb->Get_Heights();

    const uint32 expected = (countX + 1) * (countZ + 1);
    heights.resize(expected);
    fread(heights.data(), sizeof(float), expected, f);

    vb->Flush();

    uint32 layerCount = 0;
    if (fread(&layerCount, sizeof(uint32), 1, f) == 1 && layerCount > 0)
    {
        _scene->_landscapes[idx]->Clear_Layers();
        vb->Set_LayerCount(layerCount);
        for (uint32 i = 0; i < layerCount; ++i)
        {
            uint32 nameLen = 0;
            fread(&nameLen, sizeof(uint32), 1, f);
            wstring name(nameLen, L'\0');
            fread(name.data(), sizeof(WCHAR), nameLen, f);

            auto& weights = vb->Get_WeightMap(i);
            fread(weights.data(), sizeof(float), expected, f);

            wstring fullPath = (fs::path(_resourceRoot) / L"Map" / L"LandscapeTextures" / name).wstring();
            _scene->_landscapes[idx]->Add_LayerTexture(fullPath);
            _scene->_landscapes[idx]->Flush_WeightMap(i);
        }
    }
    fclose(f);
}

void MELandscapeManager::Apply_Noise(int32 idx, const LandscapeNoiseParams& params)
{
    if (idx < 0 || idx >= static_cast<int32>(_scene->_landscapes.size()))
    {
        return;
    }

    auto vb = _scene->_landscapes[idx]->Get_VIBuffer();
    if (!vb)
    {
        return;
    }

    int perm[512] = {};
    iota(perm, perm + 256, 0);
    shuffle(perm, perm + 256, mt19937(static_cast<uint32>(params.seed)));
    for (int i = 0; i < 256; ++i)
    {
        perm[256 + i] = perm[i];
    }

    const uint32 cntX = vb->Get_CountX();
    const uint32 cntZ = vb->Get_CountZ();
    auto& heights = vb->Get_Heights();

    for (uint32 z = 0; z <= cntZ; ++z)
    {
        for (uint32 x = 0; x <= cntX; ++x)
        {
            float value = 0.f;
            float amp = 1.f;
            float freq = 1.f;
            float maxAmp = 0.f;

            for (int32 o = 0; o < params.octaves; ++o)
            {
                value += NoiseUtil::Perlin_Noise2D(x * params.scale * freq, z * params.scale * freq, perm) * amp;
                maxAmp += amp;
                amp *= params.persistence;
                freq *= 2.f;
            }

            value = (value / maxAmp) * params.amplitude;

            const uint32 i = z * (cntX + 1) + x;
            heights[i] = params.additive ? heights[i] + value : value;
        }
    }

    vb->Flush();
}

void MELandscapeManager::Apply_Erosion(int32 idx, const LandscapeErosionParams& params)
{
    if (idx < 0 || idx >= static_cast<int32>(_scene->_landscapes.size()))
    {
        return;
    }

    auto vb = _scene->_landscapes[idx]->Get_VIBuffer();
    if (!vb)
    {
        return;
    }

    const uint32 cntX = vb->Get_CountX();
    const uint32 cntZ = vb->Get_CountZ();
    auto& heights = vb->Get_Heights();

    const int32 w = static_cast<int32>(cntX + 1);
    const int32 h = static_cast<int32>(cntZ + 1);

    for (int32 iter = 0; iter < params.iterations; ++iter)
    {
        bool changed = false;
        for (int32 z = 0; z < h; ++z)
        {
            for (int32 x = 0; x < w; ++x)
            {
                const int32 i = z * w + x;
                const float cur = heights[i];

                auto Erode = [&](int32 nx, int32 nz)
                    {
                        if (nx < 0 || nx >= w || nz < 0 || nz >= h)
                        {
                            return;
                        }

                        const int32 ni = nz * w + nx;
                        const float diff = cur - heights[ni];
                        if (diff > params.talusAngle)
                        {
                            const float delta = diff * params.erosionRate * 0.5f;
                            heights[i] -= delta;
                            heights[ni] += delta;
                            changed = true;
                        }
                    };

                Erode(x - 1, z);
                Erode(x + 1, z);
                Erode(x, z - 1);
                Erode(x, z + 1);
            }
        }
        if (!changed)
        {
            break;
        }
    }
    vb->Flush();
}

void MELandscapeManager::Add_LandscapeLayer(int32 idx, const WCHAR* path)
{
    if (idx < 0 || idx >= static_cast<int32>(_scene->_landscapes.size()))
    {
        return;
    }

    _scene->_landscapes[idx]->Add_LayerTexture(wstring(path));
}

void MELandscapeManager::Modify_Landscape(int32 idx, const LandscapeConfig& cfg)
{
    if (idx < 0 || idx >= static_cast<int32>(_scene->_landscapes.size()))
    {
        return;
    }

    auto& oldActor = _scene->_landscapes[idx];
    auto oldVib = oldActor->Get_VIBuffer();

    const uint32 oldCountX = oldVib->Get_CountX();
    const uint32 oldCountZ = oldVib->Get_CountZ();

    VIBuffer_Landscape::LAND_DESC desc{};
    desc.countX = cfg.countX;
    desc.countZ = cfg.countZ;
    desc.sizeX = cfg.sizeX;
    desc.sizeZ = cfg.sizeZ;
    desc.chunkSize = cfg.chunkSize;

    auto newActor = MELandscapeActor::Create(_device, _context, desc);
    if (!newActor)
    {
        return;
    }

    newActor->Get_Transform()->Set_Position(Vec3(cfg.worldOffsetX, cfg.worldOffsetY, cfg.worldOffsetZ));

    auto newVib = newActor->Get_VIBuffer();
    const bool sameResolution = (oldCountX == newVib->Get_CountX() && oldCountZ == newVib->Get_CountZ());

    if (sameResolution)
    {
        newVib->Get_Heights() = oldVib->Get_Heights();
        newVib->Flush();
    }

    const uint32 layerCount = oldActor->Get_LayerCount();
    for (uint32 i = 0; i < layerCount; ++i)
    {
        newActor->Add_LayerTexture(oldActor->Get_LayerPath(i));
        if (sameResolution)
        {
            newVib->Get_WeightMap(i) = oldVib->Get_WeightMap(i);
            newActor->Flush_WeightMap(i);
        }
    }

    _scene->_landscapes[idx] = newActor;
    if (idx < static_cast<int32>(_scene->_landscapeConfigs.size()))
    {
        _scene->_landscapeConfigs[idx] = cfg;
    }
}

void MELandscapeManager::Do_SculptAt(ImVec2 mousePos, ImVec2 vpPos, ImVec2 vpSize, float timeDelta)
{
    if (_scene->_activeLandscapeIdx < 0 || _scene->_activeLandscapeIdx >= static_cast<int32>(_scene->_landscapes.size()))
    {
        return;
    }

    auto& actor = _scene->_landscapes[_scene->_activeLandscapeIdx];
    auto vb = actor->Get_VIBuffer();
    if (!vb)
    {
        return;
    }

    float u = (mousePos.x - vpPos.x) / vpSize.x;
    float v = (mousePos.y - vpPos.y) / vpSize.y;
    float ndcX = u * 2.f - 1.f;
    float ndcY = 1.f - v * 2.f;

    Matrix viewProjInv = (Matrix(_camera->GetView()) * Matrix(_camera->GetProj())).Invert();
    Vec3 nearP = Vec3::Transform(Vec3(ndcX, ndcY, 0.f), viewProjInv);
    Vec3 farP = Vec3::Transform(Vec3(ndcX, ndcY, 1.f), viewProjInv);

    Vec3 rayDir = farP - nearP;
    rayDir.Normalize();

    Vec3 rayOrigin{}, rayDirection{};
    rayOrigin = nearP;
    rayDirection = rayDir;

    // ray & land
    Vec3 landscapePos = actor->Get_Transform()->Get_Position();
    float stepSize = min(vb->Get_SizeX(), vb->Get_SizeZ()) * 0.5f;
    Vec3 hitPos = {};
    bool hit = false;

    float camFar = _camera->GetFar();
    for (float i = 0.f; i < camFar; i += stepSize)
    {
        Vec3 p = {
          rayOrigin.x + rayDirection.x * i,
          rayOrigin.y + rayDirection.y * i,
          rayOrigin.z + rayDirection.z * i
        };

        float localX = p.x - landscapePos.x;
        float localZ = p.z - landscapePos.z;
        float terrainY = vb->Sample_Height(localX, localZ) + landscapePos.y;

        if (p.y <= terrainY)
        {
            hitPos = p;
            hit = true;
            break;
        }
    }

    if (!hit)
    {
        return;
    }

    const float radius = _sculptState->brushRadius;
    const float strength = _sculptState->brushStrength;
    const ESculptMode mode = _sculptState->mode;
    const float flatH = _sculptState->flattenHeight;

    const uint32 numVX = vb->Get_CountX() + 1;
    const uint32 numVZ = vb->Get_CountZ() + 1;
    auto& heights = vb->Get_Heights();

    float localHitX = hitPos.x - landscapePos.x;
    float localHitZ = hitPos.z - landscapePos.z;

    if (mode == ESculptMode::Paint)
    {
        vb->Paint_Layer(_sculptState->paintLayerIdx, localHitX, localHitZ, radius, strength * timeDelta * 10.f);
        actor->Flush_WeightMap(_sculptState->paintLayerIdx);
        return;
    }

    if (mode == ESculptMode::Smooth)
    {
        vector<float> smoothed = heights;
        for (uint32 z = 0; z < numVZ; ++z)
        {
            for (uint32 x = 0; x < numVX; ++x)
            {
                float vx = x * vb->Get_SizeX();
                float vz = z * vb->Get_SizeZ();
                float dist = sqrtf((vx - localHitX) * (vx - localHitX) + (vz - localHitZ) * (vz - localHitZ));

                if (dist >= radius)
                {
                    continue;
                }

                float sum = 0.f;
                int cnt = 0;
                for (int dz = -1; dz <= 1; ++dz)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        int nx = (int)x + dx;
                        int nz = (int)z + dz;
                        if (nx >= 0 && nx < (int)numVX && nz >= 0 && nz < (int)numVZ)
                        {
                            sum += heights[nz * numVX + nx];
                            ++cnt;
                        }
                    }
                }

                float falloff = 1.f - (dist / radius);
                float avg = sum / cnt;
                uint32 idx = z * numVX + x;
                smoothed[idx] = heights[idx] + (avg - heights[idx]) * falloff * strength * timeDelta * 10.f;
            }
        }
        heights = smoothed;
    }
    else
    {
        for (uint32 z = 0; z < numVZ; ++z)
        {
            for (uint32 x = 0; x < numVX; ++x)
            {
                float vx = x * vb->Get_SizeX();
                float vz = z * vb->Get_SizeZ();
                float dist = sqrtf((vx - localHitX) * (vx - localHitX) + (vz - localHitZ) * (vz - localHitZ));
                if (dist >= radius)
                {
                    continue;
                }

                float falloff = 1.f - (dist / radius);
                float delta = strength * falloff * timeDelta * 10.f;
                uint32  idx = z * numVX + x;

                switch (mode)
                {
                case ESculptMode::Raise:
                    heights[idx] += delta;
                    break;
                case ESculptMode::Lower:
                    heights[idx] -= delta;
                    break;
                case ESculptMode::Flatten:
                    heights[idx] += (flatH - heights[idx]) * delta;
                    break;
                default:
                    break;
                }
            }
        }
    }
    vb->Flush();
}

Unique<MELandscapeManager> MELandscapeManager::Create(const LandMG_Desc& desc)
{
    auto instance = make_unique<MELandscapeManager>();
    if (FAILED(instance->Initialize(desc)))
    {
        return nullptr;
    }

    return instance;
}
