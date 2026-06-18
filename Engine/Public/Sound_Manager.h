#pragma once

#include "Base.h"
#include "FMOD/fmod.h"

NS_BEGIN(Engine)

class ENGINE_DLL Sound_Manager : public Base
{
public:
    struct FFadeChannelInfo
    {
        FMOD_CHANNEL* channel = nullptr;
        float          currentVolume = 0.f;
        float          fadeSpeed = 0.f;
        SoundChannel   channelType = SoundChannel::Effect;
    };

    struct SoundCueDesc
    {
        wstring cueId{};
        vector<wstring> sounds{};

        SoundChannel channel = SoundChannel::Effect;

        bool spatial = false;       // true면 worldPosition 기준 3D 감쇠를 적용
        float minDistance = 2.f;
        float maxDistance = 40.f;

        float volumeMin = 1.f;
        float volumeMax = 1.f;
        float pitchMin = 1.f;
        float pitchMax = 1.f;

        int32 lastPlayedIndex = -1; // 같은 cue에서 직전 wav가 반복 막기용
    };

public:
    explicit Sound_Manager();
    virtual ~Sound_Manager() = default;

public:
    HRESULT Initialize(const wstring& soundRootPath, int32 maxChannels = 128);
    void    Update(float timeDelta);

public:
    // 1회 재생
    bool    Play_Once(const wstring& soundFile, SoundChannel channel, float volume = 1.f);

    // 1회 재생 + 피치 조절
    bool    Play_Once_Pitched(const wstring& soundFile, SoundChannel channel, float volume, float pitch);

    // BGM 반복 재생
    bool    Play_BGM(const wstring& soundFile,
        float volume = 1.f,
        bool stopPrevBGM = true,
        float fadeOutDuration = 0.f);

    // 일반 채널 반복 재생
    bool    Play_Loop(const wstring& soundFile,
        SoundChannel channel,
        float volume = 1.f,
        bool stopPrevChannel = false,
        float fadeOutDuration = 0.f);

    void    Set_ChannelVolume(SoundChannel channel, float volume);
    float   Get_ChannelVolume(SoundChannel channel) const;
    void    Set_ChannelPaused(SoundChannel channel, bool paused);
    void    Set_AllSoundMuted(bool muted);
    bool    Is_AllSoundMuted() const { return _allSoundMuted; }
    void    Set_GlobalPitch(float pitch, bool includeBGM = false);

    void    Stop_Channel(SoundChannel channel, float fadeOutDuration = 0.f);
    void    Stop_Sound(const wstring& soundFile);
    void    Stop_All(float fadeOutDuration = 0.f);

    bool    Has_Sound(const wstring& soundFile) const;
    const wstring& Get_CurrentBGM() const { return _currentBGM; }

#pragma region 사운드 큐
public:
    bool Play_Cue(const wstring& cueId, const Vec3* worldPosition = nullptr);
    bool Play_Cue(const wstring& cueId, const Vec3* worldPosition, float volume, float pitch);
    bool Try_GetCueDefaultVolumePitch(const wstring& cueId, float& outVolume, float& outPitch) const;
    bool Play_Once_3D(
        const wstring& soundFile,
        SoundChannel channel,
        const Vec3& worldPosition,
        float volume,
        float pitch,
        float minDistance,
        float maxDistance);

    void Set_ListenerAttributes(const Vec3& position, const Vec3& forward, const Vec3& up);

private:
    void Load_SoundCues();
    fs::path Get_SoundCueTablePath() const;
    
    bool Read_BoolFlexible(const json& item, const char* key, bool defaultValue) const;
    vector<wstring> Split_SoundList(const string& value) const;
    SoundChannel Parse_SoundChannel(const string& value, SoundChannel fallback) const;
    
    int32 Select_CueSoundIndex(SoundCueDesc& cue);
    float Random_Range(float minValue, float maxValue) const;
    FMOD_VECTOR To_FMODVector(const Vec3& value) const;
#pragma endregion

private:
    HRESULT Create_ChannelGroups();
    void    Load_SoundFiles();

    wstring Normalize_SoundKey(const wstring& soundFile) const;
    FMOD_SOUND* Find_SoundByKey(const wstring& key) const;
    FMOD_SOUND* Find_SoundByLooseKey(const wstring& key) const;
    bool    Validate_Channel(SoundChannel channel) const;

    FMOD_SOUND* Find_Sound(const wstring& soundFile) const;

    void    Register_ActiveChannel(SoundChannel channel, FMOD_CHANNEL* fmodChannel);
    void    Cleanup_StoppedChannels();
    void    Cleanup_StoppedBGMChannels();

    void    Add_FadeChannel(FMOD_CHANNEL* fmodChannel, SoundChannel channelType, float fadeOutDuration);
    void    Update_FadeChannels(float timeDelta);

    void    Release_AllSounds();
    void    Release_ChannelGroups();
    void    Shutdown_FMOD();

private:
    FMOD_SYSTEM* _system = nullptr;

    array<FMOD_CHANNELGROUP*, ETOI(SoundChannel::END)> _channelGroups = {};
    array<float, ETOI(SoundChannel::END)>              _channelVolumes = {};
    float                                              _globalPitch = 1.f;
    bool                                               _globalPitchIncludesBGM = false;
    bool                                               _allSoundMuted = false;

    // 키는 Resources/Sounds 기준 상대 경로를 소문자로 정규화해서 저장
    umap<wstring, FMOD_SOUND*> _sounds;

    // 채널별 활성 재생 목록
    array<vector<FMOD_CHANNEL*>, ETOI(SoundChannel::END)> _activeChannels = {};

    // BGM은 별도 추적해서 페이드아웃/교체 관리
    vector<FMOD_CHANNEL*> _bgmChannels;
    wstring _currentBGM{};

    list<FFadeChannelInfo> _fadeChannels;

    wstring _soundRootPath;
    int32   _maxChannels = 128;

    // 사운드 큐
    umap<wstring, SoundCueDesc> _soundCues;


public:
    static Unique<Sound_Manager> Create(const wstring& soundRootPath, int32 maxChannels = 128);
    void Free() override;

};

NS_END
