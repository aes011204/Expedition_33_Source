#include "pch.h"
#include "Sound_Manager.h"

namespace
{
    string Trim_Copy(string value)
    {
        auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };

        value.erase(value.begin(), find_if(value.begin(), value.end(),
            [&](char c) { return !isSpace(static_cast<unsigned char>(c)); }));

        value.erase(find_if(value.rbegin(), value.rend(),
            [&](char c) { return !isSpace(static_cast<unsigned char>(c)); }).base(), value.end());

        return value;
    }

    string ToLower_Copy(string value)
    {
        for (char& c : value)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        return value;
    }

    void Apply_StartRatio(FMOD_SOUND* fmodSound, FMOD_CHANNEL* fmodChannel, float startRatio)
    {
        if (!fmodSound || !fmodChannel)
            return;

        const float safeRatio = clamp(startRatio, 0.f, 0.99f);
        if (safeRatio <= 0.f)
            return;

        unsigned int lengthMs = 0;
        if (FMOD_Sound_GetLength(fmodSound, &lengthMs, FMOD_TIMEUNIT_MS) != FMOD_OK || lengthMs <= 1u)
            return;

        const unsigned int startMs = min<unsigned int>(
            lengthMs - 1u,
            static_cast<unsigned int>(static_cast<float>(lengthMs) * safeRatio));

        FMOD_Channel_SetPosition(fmodChannel, startMs, FMOD_TIMEUNIT_MS);
    }
}

Sound_Manager::Sound_Manager()
{
    _channelGroups.fill(nullptr);
    _pitchShiftDsps.fill(nullptr);
    _channelVolumes.fill(1.f);
}

HRESULT Sound_Manager::Initialize(const wstring& soundRootPath, int32 maxChannels)
{
    _soundRootPath = soundRootPath;
    _maxChannels = max(1, maxChannels);

    FMOD_RESULT result = FMOD_System_Create(&_system);
    if (result != FMOD_OK || !_system)
    {
        LOG_ERROR("FMOD_System_Create failed. result={}", static_cast<int32>(result));
        return E_FAIL;
    }

    result = FMOD_System_Init(_system, _maxChannels, FMOD_INIT_NORMAL, nullptr);
    if (result != FMOD_OK)
    {
        LOG_ERROR("FMOD_System_Init failed. result={}", static_cast<int32>(result));
        Shutdown_FMOD();
        return E_FAIL;
    }

    FMOD_System_Set3DSettings(_system, 1.f, 1.f, 1.f);
    FMOD_System_Set3DNumListeners(_system, 1);

    CHECK_FAILED(Create_ChannelGroups(), E_FAIL);

    Load_SoundFiles();
    Load_SoundCues();

    return S_OK;
}

void Sound_Manager::Update(float timeDelta)
{
    if (!_system)
        return;

    FMOD_System_Update(_system);

    Update_FadeChannels(timeDelta);
    Update_BGMIntroLoopState();
    Cleanup_StoppedChannels();
    Cleanup_StoppedChannelSpeedDsps();
    Cleanup_StoppedBGMChannels();
}

bool Sound_Manager::Play_Once(const wstring& soundFile, SoundChannel channel, float volume, float startRatio)
{
    if (!_system || _allSoundMuted || !Validate_Channel(channel))
        return false;

    FMOD_SOUND* fmodSound = Find_Sound(soundFile);
    if (!fmodSound)
        return false;

    FMOD_CHANNEL* fmodChannel = nullptr;

    FMOD_RESULT result = FMOD_System_PlaySound(
        _system,
        fmodSound,
        _channelGroups[ETOI(channel)],
        TRUE,
        &fmodChannel);

    if (result != FMOD_OK || !fmodChannel)
    {
        LOG_WARN("Play_Once failed. sound='{}', result={}",
            String::ToString(soundFile), static_cast<int32>(result));
        return false;
    }

    // 일회성 사운드이므로 루프 끄기
    FMOD_Channel_SetMode(fmodChannel, FMOD_LOOP_OFF);
    FMOD_Channel_SetVolume(fmodChannel, clamp(volume, 0.f, 1.f));
    Apply_StartRatio(fmodSound, fmodChannel, startRatio);
    FMOD_Channel_SetPaused(fmodChannel, FALSE);

    Register_ActiveChannel(channel, fmodChannel);

    return true;
}

bool Sound_Manager::Play_Once_WithSpeed(const wstring& soundFile, SoundChannel channel, float volume, float speed, float startRatio)
{
    if (!_system || _allSoundMuted || !Validate_Channel(channel))
        return false;

    FMOD_SOUND* fmodSound = Find_Sound(soundFile);
    if (!fmodSound)
        return false;

    FMOD_CHANNEL* fmodChannel = nullptr;

    FMOD_RESULT result = FMOD_System_PlaySound(
        _system,
        fmodSound,
        _channelGroups[ETOI(channel)],
        TRUE,
        &fmodChannel);

    if (result != FMOD_OK || !fmodChannel)
    {
        LOG_WARN("Play_Once_WithSpeed failed. sound='{}', result={}",
            String::ToString(soundFile), static_cast<int32>(result));
        return false;
    }

    FMOD_Channel_SetMode(fmodChannel, FMOD_LOOP_OFF);
    FMOD_Channel_SetVolume(fmodChannel, clamp(volume, 0.f, 1.f));
    Apply_ChannelPlaybackSpeed(fmodChannel, speed);
    Apply_StartRatio(fmodSound, fmodChannel, startRatio);
    FMOD_Channel_SetPaused(fmodChannel, FALSE);

    Register_ActiveChannel(channel, fmodChannel);

    return true;
}

bool Sound_Manager::Play_BGM(const wstring& soundFile, float volume, bool stopPrevBGM, float fadeOutDuration)
{
    if (!_system || _allSoundMuted)
        return false;

    Clear_BGMIntroLoopState();

    FMOD_SOUND* fmodSound = Find_Sound(soundFile);
    if (!fmodSound)
        return false;

    if (stopPrevBGM)
    {
        Stop_Channel(SoundChannel::BGM, fadeOutDuration);
    }

    Cleanup_StoppedBGMChannels();

    FMOD_CHANNEL* fmodChannel = nullptr;

    FMOD_RESULT result = FMOD_System_PlaySound(
        _system,
        fmodSound,
        _channelGroups[ETOI(SoundChannel::BGM)],
        TRUE,
        &fmodChannel);

    if (result != FMOD_OK || !fmodChannel)
    {
        LOG_WARN("Play_BGM failed. sound='{}', result={}",
            String::ToString(soundFile), static_cast<int32>(result));
        return false;
    }

    // BGM은 반복 재생
    FMOD_Channel_SetMode(fmodChannel, FMOD_LOOP_NORMAL);
    FMOD_Channel_SetVolume(fmodChannel, clamp(volume, 0.f, 1.f));
    FMOD_Channel_SetPaused(fmodChannel, FALSE);

    _bgmChannels.push_back(fmodChannel);
    Register_ActiveChannel(SoundChannel::BGM, fmodChannel);
    _currentBGM = soundFile;

    return true;
}

bool Sound_Manager::Play_BGMIntroThenLoop(
    const wstring& introFile,
    const wstring& loopFile,
    float volume,
    bool stopPrevBGM,
    float fadeOutDuration)
{
    if (!_system || _allSoundMuted)
        return false;

    Clear_BGMIntroLoopState();

    FMOD_SOUND* introSound = Find_Sound(introFile);
    if (!introSound)
        return false;

    if (!Find_Sound(loopFile))
        return false;

    if (stopPrevBGM)
    {
        Stop_Channel(SoundChannel::BGM, fadeOutDuration);
    }

    Cleanup_StoppedBGMChannels();

    FMOD_CHANNEL* fmodChannel = nullptr;

    FMOD_RESULT result = FMOD_System_PlaySound(
        _system,
        introSound,
        _channelGroups[ETOI(SoundChannel::BGM)],
        TRUE,
        &fmodChannel);

    if (result != FMOD_OK || !fmodChannel)
    {
        LOG_WARN("Play_BGMIntroThenLoop failed. sound='{}', result={}",
            String::ToString(introFile), static_cast<int32>(result));
        return false;
    }

    FMOD_Channel_SetMode(fmodChannel, FMOD_LOOP_OFF);
    FMOD_Channel_SetVolume(fmodChannel, clamp(volume, 0.f, 1.f));
    FMOD_Channel_SetPaused(fmodChannel, FALSE);

    _bgmChannels.push_back(fmodChannel);
    Register_ActiveChannel(SoundChannel::BGM, fmodChannel);
    _currentBGM = introFile;

    _bgmIntroChannel = fmodChannel;
    _bgmIntroLoopFile = loopFile;
    _bgmIntroLoopVolume = volume;

    return true;
}

bool Sound_Manager::Play_Loop(const wstring& soundFile, SoundChannel channel, float volume, bool stopPrevChannel,
    float fadeOutDuration)
{
    if (!_system || _allSoundMuted || !Validate_Channel(channel))
        return false;

    FMOD_SOUND* fmodSound = Find_Sound(soundFile);
    if (!fmodSound)
        return false;

    if (stopPrevChannel)
    {
        Stop_Channel(channel, fadeOutDuration);
    }

    FMOD_CHANNEL* fmodChannel = nullptr;

    FMOD_RESULT result = FMOD_System_PlaySound(
        _system,
        fmodSound,
        _channelGroups[ETOI(channel)],
        TRUE,
        &fmodChannel);

    if (result != FMOD_OK || !fmodChannel)
    {
        LOG_WARN("Play_Loop failed. sound='{}', result={}",
            String::ToString(soundFile), static_cast<int32>(result));
        return false;
    }

    FMOD_Channel_SetMode(fmodChannel, FMOD_LOOP_NORMAL);
    FMOD_Channel_SetVolume(fmodChannel, clamp(volume, 0.f, 1.f));
    FMOD_Channel_SetPaused(fmodChannel, FALSE);

    Register_ActiveChannel(channel, fmodChannel);

    if (channel == SoundChannel::BGM)
        _bgmChannels.push_back(fmodChannel);

    return true;
}

void Sound_Manager::Set_ChannelVolume(SoundChannel channel, float volume)
{
    if (!_system || !Validate_Channel(channel))
        return;

    const uint32 index = ETOI(channel);
    _channelVolumes[index] = clamp(volume, 0.f, 1.f);

    if (_channelGroups[index])
    {
        FMOD_ChannelGroup_SetVolume(_channelGroups[index], _channelVolumes[index]);
    }
}

float Sound_Manager::Get_ChannelVolume(SoundChannel channel) const
{
    if (!Validate_Channel(channel))
        return 0.f;

    return _channelVolumes[ETOI(channel)];
}

void Sound_Manager::Set_ChannelPaused(SoundChannel channel, bool paused)
{
    if (!_system || !Validate_Channel(channel))
        return;

    const uint32 index = ETOI(channel);
    const FMOD_BOOL fmodPaused = paused ? TRUE : FALSE;

    if (_channelGroups[index])
        FMOD_ChannelGroup_SetPaused(_channelGroups[index], fmodPaused);

    for (FMOD_CHANNEL* fmodChannel : _activeChannels[index])
    {
        if (fmodChannel == nullptr)
            continue;

        FMOD_BOOL isPlaying = FALSE;
        if (FMOD_Channel_IsPlaying(fmodChannel, &isPlaying) != FMOD_OK || !isPlaying)
            continue;

        FMOD_Channel_SetPaused(fmodChannel, fmodPaused);
    }
}

void Sound_Manager::Set_AllSoundMuted(bool muted)
{
    _allSoundMuted = muted;

    if (!_system)
        return;

    const FMOD_BOOL fmodMuted = _allSoundMuted ? TRUE : FALSE;
    for (uint32 i = 0; i < ETOI(SoundChannel::END); ++i)
    {
        if (_channelGroups[i])
            FMOD_ChannelGroup_SetMute(_channelGroups[i], fmodMuted);
    }
}

void Sound_Manager::Set_GlobalPitch(float pitch, bool includeBGM)
{
    _globalPitch = max(0.01f, pitch);
    _globalPitchIncludesBGM = includeBGM;

    for (uint32 i = 0; i < ETOI(SoundChannel::END); ++i)
    {
        const SoundChannel channel = static_cast<SoundChannel>(i);
        const float channelPitch = (channel == SoundChannel::BGM && !_globalPitchIncludesBGM) ? 1.f : _globalPitch;
        Apply_ChannelPlaybackSpeed(channel, channelPitch);
    }
}

void Sound_Manager::Stop_Channel(SoundChannel channel, float fadeOutDuration)
{
    if (!_system || !Validate_Channel(channel))
        return;

    const uint32 index = ETOI(channel);

    if (fadeOutDuration > 0.f)
    {
        for (FMOD_CHANNEL* fmodChannel : _activeChannels[index])
        {
            Add_FadeChannel(fmodChannel, channel, fadeOutDuration);
        }

        _activeChannels[index].clear();

        if (channel == SoundChannel::BGM)
        {
            Clear_BGMIntroLoopState();
            _bgmChannels.clear();
            _currentBGM.clear();
        }

        return;
    }

    if (_channelGroups[index])
    {
        FMOD_ChannelGroup_Stop(_channelGroups[index]);
    }

    _activeChannels[index].clear();

    if (channel == SoundChannel::BGM)
    {
        Clear_BGMIntroLoopState();
        _bgmChannels.clear();
        _currentBGM.clear();
    }
}

void Sound_Manager::Stop_Sound(const wstring& soundFile)
{
    if (!_system)
        return;

    FMOD_SOUND* targetSound = Find_Sound(soundFile);
    if (!targetSound)
        return;

    for (uint32 channelIndex = 0; channelIndex < ETOI(SoundChannel::END); ++channelIndex)
    {
        auto& activeList = _activeChannels[channelIndex];

        for (FMOD_CHANNEL* fmodChannel : activeList)
        {
            if (!fmodChannel)
                continue;

            FMOD_BOOL isPlaying = FALSE;
            if (FMOD_Channel_IsPlaying(fmodChannel, &isPlaying) != FMOD_OK || !isPlaying)
                continue;

            FMOD_SOUND* currentSound = nullptr;
            FMOD_Channel_GetCurrentSound(fmodChannel, &currentSound);

            if (currentSound == targetSound)
            {
                FMOD_Channel_Stop(fmodChannel);
            }
        }
    }

    Cleanup_StoppedChannels();
    Cleanup_StoppedBGMChannels();
}

void Sound_Manager::Stop_All(float fadeOutDuration)
{
    for (uint32 i = 0; i < ETOI(SoundChannel::END); ++i)
    {
        Stop_Channel(static_cast<SoundChannel>(i), fadeOutDuration);
    }
}

bool Sound_Manager::Has_Sound(const wstring& soundFile) const
{
    const wstring key = Normalize_SoundKey(soundFile);

    return Find_SoundByKey(key) != nullptr ||
        Find_SoundByLooseKey(key) != nullptr;
}

bool Sound_Manager::Play_Cue(const wstring& cueId, const Vec3* worldPosition)
{
    auto iter = _soundCues.find(Normalize_SoundKey(cueId));
    if (iter == _soundCues.end())
    {
        LOG_WARN("Sound cue not found: {}", String::ToString(cueId));
        return false;
    }

    SoundCueDesc& cue = iter->second;
    const int32 index = Select_CueSoundIndex(cue);
    if (index < 0)
        return false;

    const wstring& soundFile = cue.sounds[static_cast<size_t>(index)];
    const float volume = Random_Range(cue.volumeMin, cue.volumeMax);
    const float speed = Random_Range(cue.speedMin, cue.speedMax);

    if (cue.spatial && worldPosition != nullptr)
    {
        return Play_Once_3D(
            soundFile,
            cue.channel,
            *worldPosition,
            volume,
            speed,
            cue.minDistance,
            cue.maxDistance);
    }

    if (speed == 1.f)
        return Play_Once(soundFile, cue.channel, volume);

    return Play_Once_WithSpeed(soundFile, cue.channel, volume, speed);
}

bool Sound_Manager::Play_Cue(const wstring& cueId, const Vec3* worldPosition, float volume, float speed, float startRatio)
{
    auto iter = _soundCues.find(Normalize_SoundKey(cueId));
    if (iter == _soundCues.end())
    {
        LOG_WARN("Sound cue not found: {}", String::ToString(cueId));
        return false;
    }

    SoundCueDesc& cue = iter->second;
    const int32 index = Select_CueSoundIndex(cue);
    if (index < 0)
        return false;

    const wstring& soundFile = cue.sounds[static_cast<size_t>(index)];

    if (cue.spatial && worldPosition != nullptr)
    {
        return Play_Once_3D(
            soundFile,
            cue.channel,
            *worldPosition,
            volume,
            speed,
            cue.minDistance,
            cue.maxDistance,
            startRatio);
    }

    if (speed == 1.f)
        return Play_Once(soundFile, cue.channel, volume, startRatio);

    return Play_Once_WithSpeed(soundFile, cue.channel, volume, speed, startRatio);
}

bool Sound_Manager::Try_GetCueDefaultVolumeSpeed(const wstring& cueId, float& outVolume, float& outSpeed) const
{
    auto iter = _soundCues.find(Normalize_SoundKey(cueId));
    if (iter == _soundCues.end())
        return false;

    const SoundCueDesc& cue = iter->second;

    float volumeMin = cue.volumeMin;
    float volumeMax = cue.volumeMax;
    if (volumeMax < volumeMin)
        std::swap(volumeMin, volumeMax);

    float speedMin = cue.speedMin;
    float speedMax = cue.speedMax;
    if (speedMax < speedMin)
        std::swap(speedMin, speedMax);

    outVolume = (volumeMin + volumeMax) * 0.5f; // notify 기본값은 cue 랜덤 범위의 중앙값으로 초기화한다.
    outSpeed = (speedMin + speedMax) * 0.5f;
    return true;
}

bool Sound_Manager::Play_Once_3D(const wstring& soundFile, SoundChannel channel, const Vec3& worldPosition,
    float volume, float speed, float minDistance, float maxDistance, float startRatio)
{
    if (!_system || _allSoundMuted || !Validate_Channel(channel))
        return false;

    FMOD_SOUND* fmodSound = Find_Sound(soundFile);
    if (!fmodSound)
        return false;

    FMOD_CHANNEL* fmodChannel = nullptr;
    FMOD_RESULT result = FMOD_System_PlaySound(
        _system,
        fmodSound,
        _channelGroups[ETOI(channel)],
        TRUE,
        &fmodChannel);

    if (result != FMOD_OK || !fmodChannel)
        return false;

    FMOD_VECTOR pos = To_FMODVector(worldPosition);
    FMOD_VECTOR vel = { 0.f, 0.f, 0.f };

    FMOD_Channel_SetMode(fmodChannel, FMOD_LOOP_OFF | FMOD_3D | FMOD_3D_WORLDRELATIVE | FMOD_3D_LINEARROLLOFF);
    FMOD_Channel_Set3DAttributes(fmodChannel, &pos, &vel);
    FMOD_Channel_Set3DMinMaxDistance(fmodChannel, max(0.01f, minDistance), max(minDistance, maxDistance));
    FMOD_Channel_SetVolume(fmodChannel, clamp(volume, 0.f, 1.f));
    Apply_ChannelPlaybackSpeed(fmodChannel, speed);
    Apply_StartRatio(fmodSound, fmodChannel, startRatio);
    FMOD_Channel_SetPaused(fmodChannel, FALSE);

    Register_ActiveChannel(channel, fmodChannel);
    return true;
}

void Sound_Manager::Set_ListenerAttributes(const Vec3& position, const Vec3& forward, const Vec3& up)
{
    if (!_system)
        return;

    Vec3 safeForward = forward;
    Vec3 safeUp = up;

    if (safeForward.LengthSquared() <= 0.0001f)
        safeForward = Vec3::Forward;
    else
        safeForward.Normalize();

    if (safeUp.LengthSquared() <= 0.0001f)
        safeUp = Vec3::Up;
    else
        safeUp.Normalize();

    FMOD_VECTOR listenerPos = To_FMODVector(position);
    FMOD_VECTOR listenerVel = { 0.f, 0.f, 0.f };
    FMOD_VECTOR listenerForward = To_FMODVector(safeForward);
    FMOD_VECTOR listenerUp = To_FMODVector(safeUp);

    FMOD_RESULT result = FMOD_System_Set3DListenerAttributes(
        _system,
        0,
        &listenerPos,
        &listenerVel,
        &listenerForward,
        &listenerUp);

    if (result != FMOD_OK)
    {
        LOG_WARN("Set_ListenerAttributes failed. result={}", static_cast<int32>(result));
    }
}

void Sound_Manager::Load_SoundCues()
{
    _soundCues.clear();

    const fs::path tablePath = Get_SoundCueTablePath();
    if (!fs::exists(tablePath))
    {
        LOG_INFO("Sound cue table not found: {}", String::ToString(tablePath.wstring()));
        return;
    }

    json root{};

    try
    {
        ifstream file(tablePath);
        if (!file.is_open())
        {
            LOG_WARN("Failed to open sound cue table: {}", String::ToString(tablePath.wstring()));
            return;
        }

        file >> root;
    }
    catch (const std::exception& e)
    {
        LOG_WARN("Failed to parse sound cue table: {} ({})",
            String::ToString(tablePath.wstring()), e.what());
        return;
    }

    if (!root.contains("DT_SoundCue") || !root["DT_SoundCue"].is_array())
    {
        LOG_WARN("DT_SoundCue array not found: {}", String::ToString(tablePath.wstring()));
        return;
    }

    for (const json& item : root["DT_SoundCue"])
    {
        const string cueIdText = Trim_Copy(item.value("cueId", ""));
        if (cueIdText.empty())
            continue;

        SoundCueDesc cue{};
        cue.cueId = String::ToWString(cueIdText);
        cue.channel = Parse_SoundChannel(item.value("channel", ""), SoundChannel::Effect);
        cue.spatial = Read_BoolFlexible(item, "spatial", false);
        cue.minDistance = item.value("minDistance", cue.minDistance);
        cue.maxDistance = item.value("maxDistance", cue.maxDistance);
        cue.volumeMin = item.value("volumeMin", cue.volumeMin);
        cue.volumeMax = item.value("volumeMax", cue.volumeMax);
        cue.speedMin = item.value("speedMin", item.value("pitchMin", cue.speedMin));
        cue.speedMax = item.value("speedMax", item.value("pitchMax", cue.speedMax));
        cue.sounds = Split_SoundList(item.value("sounds", ""));

        if (cue.sounds.empty())
        {
            LOG_WARN("Sound cue has no sounds: {}", cueIdText);
            continue;
        }

        if (cue.maxDistance < cue.minDistance)
            std::swap(cue.minDistance, cue.maxDistance);

        if (cue.volumeMax < cue.volumeMin)
            std::swap(cue.volumeMin, cue.volumeMax);

        if (cue.speedMax < cue.speedMin)
            std::swap(cue.speedMin, cue.speedMax);

        cue.volumeMin = clamp(cue.volumeMin, 0.f, 1.f);
        cue.volumeMax = clamp(cue.volumeMax, 0.f, 1.f);
        cue.speedMin = max(0.01f, cue.speedMin);
        cue.speedMax = max(0.01f, cue.speedMax);
        cue.minDistance = max(0.01f, cue.minDistance);
        cue.maxDistance = max(cue.minDistance, cue.maxDistance);

        for (const wstring& soundFile : cue.sounds)
        {
            if (!Has_Sound(soundFile))
            {
                LOG_WARN("Sound cue references missing sound. cue='{}', sound='{}'",
                    cueIdText, String::ToString(soundFile));
            }
        }

        _soundCues.emplace(Normalize_SoundKey(cue.cueId), std::move(cue));
    }

    LOG_INFO("Sound cues loaded: {}", static_cast<int32>(_soundCues.size()));
}

HRESULT Sound_Manager::Reload_SoundCues()
{
    if (_system == nullptr)
        return E_FAIL;

    Load_SoundFiles(); // 에디터 실행 중 추가된 wav도 SoundCue 검증/재생에 잡히도록 캐시에 더한다.
    Load_SoundCues();  // DT_SoundCue.json을 다시 읽어서 _soundCues 캐시를 갱신한다.
    return S_OK;
}

fs::path Sound_Manager::Get_SoundCueTablePath() const
{
    const fs::path soundRoot = _soundRootPath;
    return soundRoot.parent_path() / L"Data" / L"json" / L"DT_SoundCue.json";
}

bool Sound_Manager::Read_BoolFlexible(const json& item, const char* key, bool defaultValue) const
{
    if (!item.contains(key) || item[key].is_null())
        return defaultValue;

    const json& value = item[key];

    if (value.is_boolean())
        return value.get<bool>();

    if (value.is_number_integer())
        return value.get<int>() != 0;

    if (value.is_number_float())
        return value.get<float>() != 0.f;

    if (value.is_string())
    {
        const string text = ToLower_Copy(Trim_Copy(value.get<string>()));

        if (text == "true" || text == "1" || text == "yes" || text == "y")
            return true;

        if (text == "false" || text == "0" || text == "no" || text == "n")
            return false;
    }

    LOG_WARN("Invalid bool field in sound cue table. key={}, valueType={}", key, value.type_name());
    return defaultValue;
}

vector<wstring> Sound_Manager::Split_SoundList(const string& value) const
{
    vector<wstring> result{};

    size_t begin = 0;
    while (begin <= value.size())
    {
        const size_t end = value.find('|', begin);
        const string token = Trim_Copy(value.substr(
            begin,
            end == string::npos ? string::npos : end - begin));

        if (!token.empty())
            result.push_back(String::ToWString(token));

        if (end == string::npos)
            break;

        begin = end + 1;
    }

    return result;
}

SoundChannel Sound_Manager::Parse_SoundChannel(const string& value, SoundChannel fallback) const
{
    string token = Trim_Copy(value);
    if (token.empty())
        return fallback;

    constexpr string_view prefix = "SoundChannel::";
    if (token.rfind(prefix.data(), 0) == 0)
        token = token.substr(prefix.size());

    if (const auto parsed = magic_enum::enum_cast<SoundChannel>(token); parsed.has_value())
        return parsed.value();

    const string lowerToken = ToLower_Copy(token);
    for (const auto& [enumValue, enumName] : magic_enum::enum_entries<SoundChannel>())
    {
        if (ToLower_Copy(string(enumName)) == lowerToken)
            return enumValue;
    }

    LOG_WARN("Invalid SoundChannel in sound cue table: {}", value);
    return fallback;
}

int32 Sound_Manager::Select_CueSoundIndex(SoundCueDesc& cue)
{
    const int32 count = static_cast<int32>(cue.sounds.size());
    if (count <= 0)
        return -1;

    if (count == 1)
    {
        cue.lastPlayedIndex = 0;
        return 0;
    }

    int32 index = rand() % count;
    if (index == cue.lastPlayedIndex)
        index = (index + 1 + (rand() % (count - 1))) % count;

    cue.lastPlayedIndex = index;
    return index;
}

float Sound_Manager::Random_Range(float minValue, float maxValue) const
{
    if (maxValue < minValue)
        std::swap(minValue, maxValue);

    if (minValue == maxValue)
        return minValue;

    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> dist(minValue, maxValue);
    return dist(rng);
}

FMOD_VECTOR Sound_Manager::To_FMODVector(const Vec3& value) const
{
    return FMOD_VECTOR{ value.x, value.y, value.z };
}

HRESULT Sound_Manager::Create_ChannelGroups()
{
    static const array<const char*, ETOI(SoundChannel::END)> s_groupNames =
    {
        "BGM",
        "Effect",
        "UI",
        "Player",
        "Monster",
        "Boss",
        "Ambient",
        "Voice",
        "System"
    };

    for (uint32 i = 0; i < ETOI(SoundChannel::END); ++i)
    {
        FMOD_RESULT result = FMOD_System_CreateChannelGroup(
            _system,
            s_groupNames[i],
            &_channelGroups[i]);

        if (result != FMOD_OK || !_channelGroups[i])
        {
            LOG_ERROR("Create_ChannelGroups failed. index={}, result={}", i, static_cast<int32>(result));
            return E_FAIL;
        }

        FMOD_ChannelGroup_SetVolume(_channelGroups[i], _channelVolumes[i]);
        FMOD_ChannelGroup_SetMute(_channelGroups[i], _allSoundMuted ? TRUE : FALSE);

        FMOD_DSP* pitchShiftDsp = nullptr;
        result = FMOD_System_CreateDSPByType(_system, FMOD_DSP_TYPE_PITCHSHIFT, &pitchShiftDsp);
        if (result == FMOD_OK && pitchShiftDsp)
        {
            FMOD_DSP_SetParameterFloat(pitchShiftDsp, FMOD_DSP_PITCHSHIFT_PITCH, 1.f);
            FMOD_DSP_SetBypass(pitchShiftDsp, TRUE);

            result = FMOD_ChannelGroup_AddDSP(_channelGroups[i], 0, pitchShiftDsp);
            if (result == FMOD_OK)
            {
                _pitchShiftDsps[i] = pitchShiftDsp;
            }
            else
            {
                LOG_WARN("Channel pitch-shift DSP attach failed. index={}, result={}", i, static_cast<int32>(result));
                FMOD_DSP_Release(pitchShiftDsp);
            }
        }
        else
        {
            LOG_WARN("Channel pitch-shift DSP create failed. index={}, result={}", i, static_cast<int32>(result));
        }
    }

    Set_GlobalPitch(_globalPitch, _globalPitchIncludesBGM);

    return S_OK;
}

void Sound_Manager::Apply_ChannelPlaybackSpeed(SoundChannel channel, float speed)
{
    if (!Validate_Channel(channel))
        return;

    const uint32 index = ETOI(channel);
    if (!_channelGroups[index])
        return;

    const float safeSpeed = max(0.01f, speed);

    // ChannelGroup pitch changes playback speed. PitchShift DSP cancels the audible pitch change.
    FMOD_ChannelGroup_SetPitch(_channelGroups[index], safeSpeed);

    FMOD_DSP* pitchShiftDsp = _pitchShiftDsps[index];
    if (!pitchShiftDsp)
        return;

    const bool needsPitchCompensation = fabsf(safeSpeed - 1.f) > 0.001f;
    FMOD_DSP_SetBypass(pitchShiftDsp, needsPitchCompensation ? FALSE : TRUE);
    FMOD_DSP_SetParameterFloat(pitchShiftDsp, FMOD_DSP_PITCHSHIFT_PITCH, 1.f / safeSpeed);
}

void Sound_Manager::Apply_ChannelPlaybackSpeed(FMOD_CHANNEL* fmodChannel, float speed)
{
    if (!fmodChannel)
        return;

    const float safeSpeed = max(0.01f, speed);

    // Per-sound channel speed is compensated here so AN_PlaySound Speed does not raise pitch.
    FMOD_Channel_SetPitch(fmodChannel, safeSpeed);

    if (fabsf(safeSpeed - 1.f) <= 0.001f)
        return;

    FMOD_DSP* pitchShiftDsp = nullptr;
    FMOD_RESULT result = FMOD_System_CreateDSPByType(_system, FMOD_DSP_TYPE_PITCHSHIFT, &pitchShiftDsp);
    if (result != FMOD_OK || !pitchShiftDsp)
    {
        LOG_WARN("Channel speed pitch-shift DSP create failed. result={}", static_cast<int32>(result));
        return;
    }

    FMOD_DSP_SetParameterFloat(pitchShiftDsp, FMOD_DSP_PITCHSHIFT_PITCH, 1.f / safeSpeed);

    result = FMOD_Channel_AddDSP(fmodChannel, 0, pitchShiftDsp);
    if (result != FMOD_OK)
    {
        LOG_WARN("Channel speed pitch-shift DSP attach failed. result={}", static_cast<int32>(result));
        FMOD_DSP_Release(pitchShiftDsp);
        return;
    }

    FChannelSpeedDspInfo dspInfo{};
    dspInfo.channel = fmodChannel;
    dspInfo.dsp = pitchShiftDsp;
    _channelSpeedDsps.push_back(dspInfo);
}

void Sound_Manager::Load_SoundFiles()
{
    if (_soundRootPath.empty())
        return;

    fs::path rootPath = _soundRootPath;
    if (!fs::exists(rootPath))
    {
        LOG_WARN("Sound root path not found: {}", String::ToString(_soundRootPath));
        return;
    }

    for (const auto& entry : fs::recursive_directory_iterator(rootPath))
    {
        if (!entry.is_regular_file())
            continue;

        wstring extension = entry.path().extension().wstring();
        transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

        if (extension != L".wav" &&
            extension != L".ogg" &&
            extension != L".mp3")
        {
            continue;
        }

        wstring key = fs::relative(entry.path(), rootPath).generic_wstring();
        transform(key.begin(), key.end(), key.begin(), ::towlower);

        if (_sounds.find(key) != _sounds.end())
            continue;

        FMOD_MODE mode = FMOD_DEFAULT;

        auto relativePath = fs::relative(entry.path(), rootPath);
        if (relativePath.begin() != relativePath.end())
        {
            wstring topFolder = relativePath.begin()->wstring();
            transform(topFolder.begin(), topFolder.end(), topFolder.begin(), ::towupper);

            // BGM은 스트리밍으로 다루는 편이 메모리 사용량에 유리하다.
            if (topFolder == L"BGM")
                mode |= FMOD_CREATESTREAM;
        }

        FMOD_SOUND* fmodSound = nullptr;
        std::string utf8Path = String::ToString(entry.path().wstring());

        FMOD_RESULT result = FMOD_System_CreateSound(
            _system,
            utf8Path.c_str(),
            mode,
            nullptr,
            &fmodSound);

        if (result == FMOD_OK && fmodSound)
        {
            _sounds.emplace(key, fmodSound);
        }
        else
        {
            LOG_WARN("Failed to load sound file: {}", String::ToString(entry.path().wstring()));
        }
    }

    LOG_INFO("Sound files loaded: {}", static_cast<int32>(_sounds.size()));
}

wstring Sound_Manager::Normalize_SoundKey(const wstring& soundFile) const
{
    wstring key = soundFile;

    if (!key.empty())
    {
        std::replace(key.begin(), key.end(), L'\\', L'/');
        transform(key.begin(), key.end(), key.begin(), ::towlower);
    }

    return key;
}

bool Sound_Manager::Validate_Channel(SoundChannel channel) const
{
    return channel >= SoundChannel::BGM && channel < SoundChannel::END;
}

FMOD_SOUND* Sound_Manager::Find_SoundByKey(const wstring& key) const
{
    auto iter = _sounds.find(key);
    if (iter != _sounds.end())
        return iter->second;

    const fs::path keyPath(key);
    if (keyPath.has_extension())
        return nullptr;

    static constexpr array<wstring_view, 3> kExtensions = { L".wav", L".ogg", L".mp3" };
    for (wstring_view extension : kExtensions)
    {
        iter = _sounds.find(key + wstring(extension));
        if (iter != _sounds.end())
            return iter->second;
    }

    return nullptr;
}

FMOD_SOUND* Sound_Manager::Find_SoundByLooseKey(const wstring& key) const
{
    if (key.find(L'/') != wstring::npos)
        return nullptr;

    FMOD_SOUND* found = nullptr;
    int32 matchCount = 0;

    for (const auto& [soundKey, sound] : _sounds)
    {
        const fs::path soundPath(soundKey);
        const wstring filename = soundPath.filename().generic_wstring();
        const wstring stem = soundPath.stem().generic_wstring();

        if (filename != key && stem != key)
            continue;

        found = sound;
        ++matchCount;
    }

    if (matchCount > 1)
    {
        LOG_WARN("Sound key is ambiguous: {}", String::ToString(key));
        return nullptr;
    }

    return found;
}

FMOD_SOUND* Sound_Manager::Find_Sound(const wstring& soundFile) const
{
    const wstring key = Normalize_SoundKey(soundFile);

    if (FMOD_SOUND* sound = Find_SoundByKey(key))
        return sound;

    if (FMOD_SOUND* sound = Find_SoundByLooseKey(key))
        return sound;

    LOG_WARN("Sound not found: {}", String::ToString(soundFile));
    return nullptr;
}

void Sound_Manager::Register_ActiveChannel(SoundChannel channel, FMOD_CHANNEL* fmodChannel)
{
    if (!fmodChannel || !Validate_Channel(channel))
        return;

    _activeChannels[ETOI(channel)].push_back(fmodChannel);
}

void Sound_Manager::Cleanup_StoppedChannels()
{
    for (uint32 channelIndex = 0; channelIndex < ETOI(SoundChannel::END); ++channelIndex)
    {
        auto& activeList = _activeChannels[channelIndex];

        activeList.erase(
            remove_if(activeList.begin(), activeList.end(),
                [](FMOD_CHANNEL* fmodChannel)
                {
                    if (!fmodChannel)
                        return true;

                    FMOD_BOOL isPlaying = FALSE;
                    if (FMOD_Channel_IsPlaying(fmodChannel, &isPlaying) != FMOD_OK)
                        return true;

                    return isPlaying == FALSE;
                }),
            activeList.end());
    }
}

void Sound_Manager::Cleanup_StoppedChannelSpeedDsps()
{
    auto iter = _channelSpeedDsps.begin();
    while (iter != _channelSpeedDsps.end())
    {
        bool shouldRelease = iter->channel == nullptr || iter->dsp == nullptr;

        if (!shouldRelease)
        {
            FMOD_BOOL isPlaying = FALSE;
            shouldRelease = FMOD_Channel_IsPlaying(iter->channel, &isPlaying) != FMOD_OK || isPlaying == FALSE;
        }

        if (shouldRelease)
        {
            if (iter->dsp)
                FMOD_DSP_Release(iter->dsp);

            iter = _channelSpeedDsps.erase(iter);
            continue;
        }

        ++iter;
    }
}

void Sound_Manager::Cleanup_StoppedBGMChannels()
{
    _bgmChannels.erase(
        remove_if(_bgmChannels.begin(), _bgmChannels.end(),
            [](FMOD_CHANNEL* fmodChannel)
            {
                if (!fmodChannel)
                    return true;

                FMOD_BOOL isPlaying = FALSE;
                if (FMOD_Channel_IsPlaying(fmodChannel, &isPlaying) != FMOD_OK)
                    return true;

                return isPlaying == FALSE;
            }),
        _bgmChannels.end());
}

void Sound_Manager::Clear_BGMIntroLoopState()
{
    _bgmIntroChannel = nullptr;
    _bgmIntroLoopFile.clear();
    _bgmIntroLoopVolume = 1.f;
}

void Sound_Manager::Update_BGMIntroLoopState()
{
    if (_bgmIntroChannel == nullptr || _bgmIntroLoopFile.empty())
        return;

    FMOD_BOOL isPlaying = FALSE;
    const FMOD_RESULT result = FMOD_Channel_IsPlaying(_bgmIntroChannel, &isPlaying);
    if (result == FMOD_OK && isPlaying)
        return;

    const wstring loopFile = _bgmIntroLoopFile;
    const float loopVolume = _bgmIntroLoopVolume;
    Clear_BGMIntroLoopState();

    Play_BGM(loopFile, loopVolume, true, 0.f);
}

void Sound_Manager::Add_FadeChannel(FMOD_CHANNEL* fmodChannel, SoundChannel channelType, float fadeOutDuration)
{
    if (!fmodChannel || fadeOutDuration <= 0.f)
        return;

    FMOD_BOOL isPlaying = FALSE;
    if (FMOD_Channel_IsPlaying(fmodChannel, &isPlaying) != FMOD_OK || !isPlaying)
        return;

    float currentVolume = 0.f;
    FMOD_Channel_GetVolume(fmodChannel, &currentVolume);

    if (currentVolume <= 0.f)
    {
        FMOD_Channel_Stop(fmodChannel);
        return;
    }

    FFadeChannelInfo fadeInfo{};
    fadeInfo.channel = fmodChannel;
    fadeInfo.currentVolume = currentVolume;
    fadeInfo.fadeSpeed = currentVolume / fadeOutDuration;
    fadeInfo.channelType = channelType;

    _fadeChannels.push_back(fadeInfo);
}

void Sound_Manager::Update_FadeChannels(float timeDelta)
{
    auto iter = _fadeChannels.begin();

    while (iter != _fadeChannels.end())
    {
        if (!iter->channel)
        {
            iter = _fadeChannels.erase(iter);
            continue;
        }

        iter->currentVolume -= iter->fadeSpeed * timeDelta;

        if (iter->currentVolume <= 0.f)
        {
            FMOD_Channel_Stop(iter->channel);
            iter = _fadeChannels.erase(iter);
        }
        else
        {
            FMOD_Channel_SetVolume(iter->channel, iter->currentVolume);
            ++iter;
        }
    }
}

void Sound_Manager::Release_AllSounds()
{
    for (auto& [key, fmodSound] : _sounds)
    {
        if (fmodSound)
        {
            FMOD_Sound_Release(fmodSound);
        }
    }

    _sounds.clear();
}

void Sound_Manager::Release_ChannelGroups()
{
    for (FChannelSpeedDspInfo& dspInfo : _channelSpeedDsps)
    {
        if (dspInfo.dsp)
            FMOD_DSP_Release(dspInfo.dsp);
    }
    _channelSpeedDsps.clear();

    for (FMOD_DSP*& pitchShiftDsp : _pitchShiftDsps)
    {
        if (pitchShiftDsp)
        {
            FMOD_DSP_Release(pitchShiftDsp);
            pitchShiftDsp = nullptr;
        }
    }

    for (FMOD_CHANNELGROUP*& fmodGroup : _channelGroups)
    {
        if (fmodGroup)
        {
            FMOD_ChannelGroup_Release(fmodGroup);
            fmodGroup = nullptr;
        }
    }
}

void Sound_Manager::Shutdown_FMOD()
{
    Release_AllSounds();
    Release_ChannelGroups();

    Clear_BGMIntroLoopState();
    _fadeChannels.clear();
    _bgmChannels.clear();

    for (auto& activeList : _activeChannels)
        activeList.clear();

    if (_system)
    {
        FMOD_System_Close(_system);
        FMOD_System_Release(_system);
        _system = nullptr;
    }
}

Unique<Sound_Manager> Sound_Manager::Create(const wstring& soundRootPath, int32 maxChannels)
{
    auto instance = make_unique<Sound_Manager>();

    if (FAILED(instance->Initialize(soundRootPath, maxChannels)))
    {
        LOG_ERROR("Failed to create Sound_Manager");
        return nullptr;
    }

    return instance;
}

void Sound_Manager::Free()
{
    Base::Free();

    Shutdown_FMOD();
}
