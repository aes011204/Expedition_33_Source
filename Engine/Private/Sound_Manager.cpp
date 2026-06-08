#include "pch.h"
#include "Sound_Manager.h"

Sound_Manager::Sound_Manager()
{
    _channelGroups.fill(nullptr);
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

    CHECK_FAILED(Create_ChannelGroups(), E_FAIL);

    Load_SoundFiles();

    return S_OK;
}

void Sound_Manager::Update(float timeDelta)
{
    if (!_system)
        return;

    FMOD_System_Update(_system);

    Update_FadeChannels(timeDelta);
    Cleanup_StoppedChannels();
    Cleanup_StoppedBGMChannels();
}

bool Sound_Manager::Play_Once(const wstring& soundFile, SoundChannel channel, float volume)
{
    if (!_system || !Validate_Channel(channel))
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
    FMOD_Channel_SetPaused(fmodChannel, FALSE);

    Register_ActiveChannel(channel, fmodChannel);

    return true;
}

bool Sound_Manager::Play_Once_Pitched(const wstring& soundFile, SoundChannel channel, float volume, float pitch)
{
    if (!_system || !Validate_Channel(channel))
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
        LOG_WARN("Play_Once_Pitched failed. sound='{}', result={}",
            String::ToString(soundFile), static_cast<int32>(result));
        return false;
    }

    FMOD_Channel_SetMode(fmodChannel, FMOD_LOOP_OFF);
    FMOD_Channel_SetVolume(fmodChannel, clamp(volume, 0.f, 1.f));
    FMOD_Channel_SetPitch(fmodChannel, max(0.01f, pitch));
    FMOD_Channel_SetPaused(fmodChannel, FALSE);

    Register_ActiveChannel(channel, fmodChannel);

    return true;
}

bool Sound_Manager::Play_BGM(const wstring& soundFile, float volume, bool stopPrevBGM, float fadeOutDuration)
{
    if (!_system)
        return false;

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

    return true;
}

bool Sound_Manager::Play_Loop(const wstring& soundFile, SoundChannel channel, float volume, bool stopPrevChannel,
    float fadeOutDuration)
{
    if (!_system || !Validate_Channel(channel))
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
            _bgmChannels.clear();

        return;
    }

    if (_channelGroups[index])
    {
        FMOD_ChannelGroup_Stop(_channelGroups[index]);
    }

    _activeChannels[index].clear();

    if (channel == SoundChannel::BGM)
        _bgmChannels.clear();
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
    }

    return S_OK;
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
