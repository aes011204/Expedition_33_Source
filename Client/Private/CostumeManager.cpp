#include "CostumeManager.h"

static constexpr CostumePresetDesc kDefaultCostumeDescs[] =
{
    // 구스타브
    {
        HeroId::Gustave, "Default", L"기본",
        L"Model_GustaveBody", L"Model_GustaveHead",
        L"Model_GustaveHair", nullptr, // L"Model_GustaveBeard",
        nullptr, // L"Model_GustaveMustache",
        nullptr
    },
    {
        HeroId::Gustave, "Baguette", L"빠게트",
        L"Model_Gustave_Body_Baguette",
        L"Model_GustaveHead", L"Model_Gustave_Baguette_Hat",
        nullptr, // L"Model_GustaveBeard",
        nullptr, // L"Model_GustaveMustache",
        L"Model_Gustave_Baguette_Glasses2"
    },
    {
        HeroId::Gustave, "Potato", L"포테이토",
        L"Model_Gustave_Body_Potato",
        L"Model_Gustave_Head_Potato",
        nullptr, // 대머리 프리셋이므로 hair 파츠를 만들지 않는다.
        nullptr, // L"Model_GustaveBeard",
        nullptr, // L"Model_GustaveMustache",
        nullptr
    },
    // 루네
    {
        HeroId::Lune, "Default", L"기본",
        L"Model_LuneBody", L"Model_LuneHead",
        L"Model_LuneHair", nullptr,
        nullptr, nullptr
    },
    {
        HeroId::Lune, "Baguette", L"빠게트",
        L"Model_Lune_Baguette_Body",
        L"Model_LuneHead", L"Model_Lune_Baguette_Hat",
        nullptr, nullptr,
        L"Model_Lune_Baguette_Glasses"
    },
    {
        HeroId::Lune, "Potato", L"포테이토",
        L"Model_Lune_Body_Potato",
        L"Model_Lune_Head_Potato",
        nullptr, // 대머리 프리셋
        nullptr, nullptr,
        nullptr
    },
    // 마엘
    {
        HeroId::Maelle, "Default", L"기본",
        L"Model_MaelleBody", L"Model_MaelleHead",
        L"Model_MaelleHair", nullptr,
        nullptr, nullptr
    },
    {
        HeroId::Maelle, "Baguette", L"빠게트",
        L"Model_Maelle_Baguette_Body",
        L"Model_MaelleHead", L"Model_Maelle_Baguette_Hat",
        nullptr, nullptr,
        nullptr
    },
    {
        HeroId::Maelle, "Potato", L"포테이토",
        L"Model_Maelle_Body_Potato",
        L"Model_Maelle_Head_Potato",
        nullptr, // 대머리 프리셋
        nullptr, nullptr,
        nullptr
    },
};

static constexpr DefaultCostumeDesc kDefaultCostumeByHero[] =
{
    { HeroId::Gustave, "Default"},
    { HeroId::Lune, "Default"},
    { HeroId::Maelle, "Default"},
};

HRESULT CostumeManager::Initialize()
{
    _costumesByHero.clear();
    _currentCostumeByHero.clear();

    // 일단 기본 옷 등록
    Register_DefaultCostumes();

    return S_OK;
}

void CostumeManager::Register_DefaultCostumes()
{
    for (const CostumePresetDesc& desc : kDefaultCostumeDescs)
    {
        CostumePreset preset = Make_Preset(desc);
        Apply_DefaultPartPlacement(preset);
        Register_Costume(preset);
    }

    for (const DefaultCostumeDesc& desc : kDefaultCostumeByHero)
    {
        if (desc.presetId == nullptr || desc.presetId[0] == '\0')
            continue;

        _currentCostumeByHero[desc.heroId] = desc.presetId;
    }
}

void CostumeManager::Register_Costume(const CostumePreset& preset)
{
    if (preset.presetId.empty())
        return;

    _costumesByHero[preset.heroId].push_back(preset);
}

CostumePreset CostumeManager::Make_Preset(const CostumePresetDesc& desc)
{
    CostumePreset preset{};
    preset.heroId = desc.heroId;
    preset.presetId = ToStringOrEmpty(desc.presetId);
    preset.displayName = ToWStringOrEmpty(desc.displayName);

    preset.parts.bodyModelTag = ToWStringOrEmpty(desc.bodyModelTag);
    preset.parts.headModelTag = ToWStringOrEmpty(desc.headModelTag);
    preset.parts.hairModelTag = ToWStringOrEmpty(desc.hairModelTag);
    preset.parts.beardModelTag = ToWStringOrEmpty(desc.beardModelTag);
    preset.parts.mustacheModelTag = ToWStringOrEmpty(desc.mustacheModelTag);
    preset.parts.glassesModelTag = ToWStringOrEmpty(desc.glassesModelTag);

    return preset;
}

void CostumeManager::Apply_DefaultPartPlacement(CostumePreset& preset)
{
    const auto ApplyHairTransform = [&preset](const Vec3& position, const Vec3& rotation)
    {
        preset.parts.hairTransform.enabled = true;
        preset.parts.hairTransform.position = position;
        preset.parts.hairTransform.rotation = rotation;
        preset.parts.hairTransform.scale = Vec3(1.f, 1.f, 1.f);
    };

    // const auto ApplyBeardTransform = [&preset](const Vec3& position, const Vec3& rotation, const Vec3& scale)
    // {
    //     preset.parts.beardTransform.enabled = true;
    //     preset.parts.beardTransform.position = position;
    //     preset.parts.beardTransform.rotation = rotation;
    //     preset.parts.beardTransform.scale = scale;
    // };

    // const auto ApplyMustacheTransform = [&preset](const Vec3& position, const Vec3& rotation, const Vec3& scale)
    // {
    //     preset.parts.mustacheTransform.enabled = true;
    //     preset.parts.mustacheTransform.position = position;
    //     preset.parts.mustacheTransform.rotation = rotation;
    //     preset.parts.mustacheTransform.scale = scale;
    // };

    if (preset.heroId == HeroId::Gustave && preset.presetId == "Default")
    {
        ApplyHairTransform(
            Vec3(20.41f, -0.17f, 0.f),
            Vec3(-90.f, 0.f, -90.f));

        // ApplyBeardTransform(
        //     Vec3(-155.9f, 0.8f, 0.f),
        //     Vec3(-90.f, 0.f, -90.f),
        //     Vec3(1.f, 1.f, 0.9999997f));

        // ApplyMustacheTransform(
        //     Vec3(-156.f, 1.f, 0.f),
        //     Vec3(-90.f, 0.f, -90.f),
        //     Vec3(1.f, 1.f, 1.f));
    }

    if (preset.heroId == HeroId::Gustave && preset.presetId == "Baguette")
    {
        ApplyHairTransform(
            Vec3(20.41f, -0.17f, 0.f),
            Vec3(-90.f, 0.f, -90.f));

        // ApplyBeardTransform(
        //     Vec3(-155.9f, 0.8f, 0.f),
        //     Vec3(-90.f, 0.f, -90.f),
        //     Vec3(1.f, 1.f, 0.9999997f));

        // ApplyMustacheTransform(
        //     Vec3(-156.f, 1.f, 0.f),
        //     Vec3(-90.f, 0.f, -90.f),
        //     Vec3(1.f, 1.f, 1.f));

        preset.parts.glassesTransform.enabled = true;
        preset.parts.glassesTransform.position = Vec3(1.41f, 1.0f, 0.f);
        preset.parts.glassesTransform.rotation = Vec3(0.f, 0.f, 0.f);
        preset.parts.glassesTransform.scale = Vec3(1.f, 1.f, 1.f);
    }

    if (preset.heroId == HeroId::Gustave && preset.presetId == "Potato")
    {
        // ApplyBeardTransform(
        //     Vec3(-155.9f, 0.8f, 0.f),
        //     Vec3(-90.f, 0.f, -90.f),
        //     Vec3(1.f, 1.f, 0.9999997f));

        // ApplyMustacheTransform(
        //     Vec3(-156.f, 1.f, 0.f),
        //     Vec3(-90.f, 0.f, -90.f),
        //     Vec3(1.f, 1.f, 1.f));
    }

    if (preset.heroId == HeroId::Lune && preset.presetId == "Baguette")
    {
        preset.parts.hairTransform.enabled = true;
        preset.parts.hairTransform.position = Vec3(18.31f, 0.43f, 0.f);
        preset.parts.hairTransform.rotation = Vec3(-90.f, 0.f, -90.f);
        preset.parts.hairTransform.scale = Vec3(1.f, 1.f, 1.f);

        preset.parts.glassesTransform.enabled = true;
        preset.parts.glassesTransform.position = Vec3(5.49f, 1.06f, -0.04f);
        preset.parts.glassesTransform.rotation = Vec3(0.f, -90.f, 0.f);
        preset.parts.glassesTransform.scale = Vec3(1.f, 1.f, 1.f);
    }

    if (preset.heroId == HeroId::Lune && preset.presetId == "Default")
    {
        ApplyHairTransform(
            Vec3(18.31f, -2.37f, 0.f),
            Vec3(-90.f, 0.f, -90.f));
    }

    if (preset.heroId == HeroId::Maelle && preset.presetId == "Default")
    {
        ApplyHairTransform(
            Vec3(6.31f, 3.64f, 0.f),
            Vec3(-90.f, 0.f, -90.f));
    }

    if (preset.heroId == HeroId::Maelle && preset.presetId == "Baguette")
    {
        ApplyHairTransform(
            Vec3(6.31f, 3.64f, 0.f),
            Vec3(-90.f, 0.f, -90.f));
    }
}

string CostumeManager::ToStringOrEmpty(const char* value)
{
    if (value == nullptr)
        return "";

    return value;
}

wstring CostumeManager::ToWStringOrEmpty(const wchar_t* value)
{
    if (value == nullptr)
        return L"";

    return value;
}

const CostumePreset* CostumeManager::Get_CurrentCostume(HeroId heroId) const
{
    const auto currentIter = _currentCostumeByHero.find(heroId);
    if (currentIter == _currentCostumeByHero.end())
        return nullptr;

    return Get_Costume(heroId, currentIter->second);
}

const CostumePreset* CostumeManager::Get_Costume(HeroId heroId, const string& presetId) const
{
    const auto costumeIter = _costumesByHero.find(heroId);
    if (costumeIter == _costumesByHero.end())
        return nullptr;

    const vector<CostumePreset>& costumes = costumeIter->second;

    for (const CostumePreset& preset : costumes)
    {
        if (preset.presetId == presetId)
            return &preset;
    }

    return nullptr;
}

const vector<CostumePreset>* CostumeManager::Get_Costumes(HeroId heroId) const
{
    const auto iter = _costumesByHero.find(heroId);
    if (iter == _costumesByHero.end())
        return nullptr;

    return &iter->second;
}

const CostumePreset* CostumeManager::Set_CurrentCostume(HeroId heroId, const string& presetId)
{
    const CostumePreset* preset = Get_Costume(heroId, presetId);
    if (preset == nullptr)
        return nullptr;

    _currentCostumeByHero[heroId] = preset->presetId;
    return preset;
}

json CostumeManager::Create_CurrentCostumeSaveJson() const
{
    json root = json::object();
    root["current"] = json::array();

    for (const auto& [heroId, presetId] : _currentCostumeByHero)
    {
        json entry = json::object();
        entry["hero_id"] = string(magic_enum::enum_name(heroId));
        entry["preset_id"] = presetId;
        root["current"].emplace_back(entry);
    }

    return root;
}

bool CostumeManager::Apply_CurrentCostumeSaveJson(const json& data)
{
    if (!data.is_object())
        return false;

    auto current = data.find("current");
    if (current == data.end() || !current->is_array())
        return false;

    bool appliedAny = false;
    for (const json& entry : *current)
    {
        if (!entry.is_object())
            continue;

        const auto heroId = magic_enum::enum_cast<HeroId>(entry.value("hero_id", string{}));
        if (!heroId.has_value())
            continue;

        const string presetId = entry.value("preset_id", string{});
        if (Set_CurrentCostume(heroId.value(), presetId) != nullptr)
            appliedAny = true;
    }

    return appliedAny;
}

const CostumePreset* CostumeManager::Toggle_NextCostume(HeroId heroId)
{
    const auto costumeIter = _costumesByHero.find(heroId);
    if (costumeIter == _costumesByHero.end())
        return nullptr;

    const vector<CostumePreset>& costumes = costumeIter->second;
    if (costumes.empty())
        return nullptr;

    string currentPresetId{};
    const auto currentIter = _currentCostumeByHero.find(heroId);
    if (currentIter != _currentCostumeByHero.end())
    {
        currentPresetId = currentIter->second;
    }

    size_t nextIndex = 0;

    for (size_t i = 0; i < costumes.size(); ++i)
    {
        if (costumes[i].presetId == currentPresetId)
        {
            nextIndex = (i + 1) % costumes.size();
            break;
        }
    }

    _currentCostumeByHero[heroId] = costumes[nextIndex].presetId;

    return &costumes[nextIndex];
}

Unique<CostumeManager> CostumeManager::Create()
{
    auto instance = make_unique<CostumeManager>();

    if (FAILED(instance->Initialize()))
    {
        instance->Free();
        return nullptr;
    }

    return instance;
}

void CostumeManager::Free()
{
    _costumesByHero.clear();
    _currentCostumeByHero.clear();

    Base::Free();
}
