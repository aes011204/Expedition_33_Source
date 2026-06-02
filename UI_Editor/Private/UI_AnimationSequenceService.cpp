#include "pch.h"
#include "UI_AnimationSequenceService.h"

#include <commdlg.h>

namespace UIEditor
{
    namespace
    {
        bool Try_CreateDirectory(const fs::path& directory)
        {
            if (directory.empty())
                return false;

            try
            {
                if (fs::exists(directory))
                    return fs::is_directory(directory);

                return fs::create_directories(directory);
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI animation sequence directory create failed: {} ({})", exception.what(), String::ToString(directory.wstring()));
                return false;
            }
        }

        fs::path Get_ModuleDirectory()
        {
            wchar_t modulePath[MAX_PATH] = {};
            if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
                return {};

            return fs::path(modulePath).parent_path();
        }

        bool Try_FindResourceRootFromStart(const fs::path& startPath, fs::path& outResourceRoot)
        {
            if (startPath.empty())
                return false;

            try
            {
                fs::path start = fs::absolute(startPath).lexically_normal();
                for (fs::path currentPath = start; !currentPath.empty(); currentPath = currentPath.parent_path())
                {
                    const fs::path candidate = (currentPath / L"Client" / L"Bin" / L"Resources").lexically_normal();
                    if (fs::exists(candidate) && fs::is_directory(candidate))
                    {
                        outResourceRoot = candidate;
                        return true;
                    }

                    if (currentPath == currentPath.parent_path())
                        break;
                }
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI animation sequence resource root search failed: {}", exception.what());
            }

            return false;
        }

        bool Ends_With(const string& value, const string& suffix)
        {
            if (value.size() < suffix.size())
                return false;

            return equal(suffix.rbegin(), suffix.rend(), value.rbegin());
        }

        string Make_DisplayName(const UI_AnimationAsset& asset, const wstring& filePath)
        {
            if (!asset.name.empty())
                return asset.name;

            string fileName = String::ToString(fs::path(filePath).filename().wstring());
            if (Ends_With(fileName, ".uianim.json"))
                fileName.resize(fileName.size() - string(".uianim.json").size());

            return fileName.empty() ? "UI Animation" : fileName;
        }
    }

    int32 UI_AnimationSequenceService::Get_TotalFrames() const
    {
        if (_sequence.clips.empty())
            return 0;

        const UI_AnimationSequenceClip& lastClip = _sequence.clips.back();
        return lastClip.startFrame + max(1, lastClip.durationFrames);
    }

    bool UI_AnimationSequenceService::Has_AnyResolvedTarget() const
    {
        for (const UI_AnimationSequenceClip& clip : _sequence.clips)
        {
            if (clip.resolvedTargetCount > 0)
                return true;
        }

        return false;
    }

    bool UI_AnimationSequenceService::Add_ClipsFromDialog(const UI_Document& document)
    {
        vector<wstring> filePaths = {};
        if (!Show_LoadClipsDialog(filePaths))
            return false;

        bool addedAnyClip = false;
        for (const wstring& filePath : filePaths)
        {
            if (Add_ClipFromFile(filePath, document))
                addedAnyClip = true;
        }

        if (!addedAnyClip)
            return false;

        Rebuild_Timing();
        Clamp_CurrentFrame();
        if (_selectedClipIndex < 0 && !_sequence.clips.empty())
            _selectedClipIndex = 0;
        return true;
    }

    void UI_AnimationSequenceService::Clear()
    {
        _sequence.clips.clear();
        _currentFrame = 0;
        _selectedClipIndex = -1;
    }

    bool UI_AnimationSequenceService::Remove_SelectedClip()
    {
        if (_selectedClipIndex < 0 || _selectedClipIndex >= static_cast<int32>(_sequence.clips.size()))
            return false;

        _sequence.clips.erase(_sequence.clips.begin() + _selectedClipIndex);
        if (_sequence.clips.empty())
            _selectedClipIndex = -1;
        else
            _selectedClipIndex = min(_selectedClipIndex, static_cast<int32>(_sequence.clips.size()) - 1);

        Rebuild_Timing();
        Clamp_CurrentFrame();
        return true;
    }

    bool UI_AnimationSequenceService::Move_SelectedClip(int32 direction)
    {
        if (direction == 0 ||
            _selectedClipIndex < 0 ||
            _selectedClipIndex >= static_cast<int32>(_sequence.clips.size()))
        {
            return false;
        }

        const int32 nextIndex = _selectedClipIndex + (direction < 0 ? -1 : 1);
        if (nextIndex < 0 || nextIndex >= static_cast<int32>(_sequence.clips.size()))
            return false;

        swap(_sequence.clips[_selectedClipIndex], _sequence.clips[nextIndex]);
        _selectedClipIndex = nextIndex;
        Rebuild_Timing();
        Clamp_CurrentFrame();
        return true;
    }

    void UI_AnimationSequenceService::Select_Clip(int32 clipIndex)
    {
        if (clipIndex < 0 || clipIndex >= static_cast<int32>(_sequence.clips.size()))
        {
            _selectedClipIndex = -1;
            return;
        }

        _selectedClipIndex = clipIndex;
    }

    void UI_AnimationSequenceService::Set_CurrentFrame(int32 frame)
    {
        const int32 totalFrames = Get_TotalFrames();
        if (totalFrames <= 0)
        {
            _currentFrame = 0;
            return;
        }

        _currentFrame = min(max(frame, 0), totalFrames);
    }

    void UI_AnimationSequenceService::Set_Fps(int32 fps)
    {
        const int32 nextFps = max(1, fps);
        if (_sequence.fps == nextFps)
            return;

        _sequence.fps = nextFps;
        Rebuild_Timing();
        Clamp_CurrentFrame();
    }

    void UI_AnimationSequenceService::Resolve_TargetObjects(const UI_Document& document)
    {
        for (UI_AnimationSequenceClip& clip : _sequence.clips)
            Resolve_TargetObjectsForClip(clip, document);
    }

    int32 UI_AnimationSequenceService::Find_ClipIndexAtFrame(int32 frame) const
    {
        if (_sequence.clips.empty())
            return -1;

        const int32 clampedFrame = min(max(frame, 0), Get_TotalFrames());
        for (int32 clipIndex = 0; clipIndex < static_cast<int32>(_sequence.clips.size()); ++clipIndex)
        {
            const UI_AnimationSequenceClip& clip = _sequence.clips[clipIndex];
            const int32 clipStartFrame = clip.startFrame;
            const int32 clipEndFrame = clip.startFrame + max(1, clip.durationFrames);
            if (clampedFrame >= clipStartFrame && clampedFrame < clipEndFrame)
                return clipIndex;
        }

        return static_cast<int32>(_sequence.clips.size()) - 1;
    }

    bool UI_AnimationSequenceService::Show_LoadClipsDialog(vector<wstring>& outFilePaths) const
    {
        vector<wchar_t> filePathBuffer(32768, L'\0');
        const wstring initialDirectory = Resolve_DefaultAnimationDirectory();

        OPENFILENAMEW openFileName = {};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.lpstrFilter = L"UI Animation Files\0*.uianim.json\0JSON Files\0*.json\0All Files\0*.*\0";
        openFileName.lpstrFile = filePathBuffer.data();
        openFileName.nMaxFile = static_cast<DWORD>(filePathBuffer.size());
        if (!initialDirectory.empty())
            openFileName.lpstrInitialDir = initialDirectory.c_str();
        openFileName.lpstrDefExt = L"uianim.json";
        openFileName.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (FALSE == GetOpenFileNameW(&openFileName))
            return false;

        const wchar_t* cursor = filePathBuffer.data();
        const wstring firstToken = cursor;
        if (firstToken.empty())
            return false;

        cursor += firstToken.size() + 1;
        if (*cursor == L'\0')
        {
            outFilePaths.push_back(firstToken);
            return true;
        }

        const fs::path directory(firstToken);
        while (*cursor != L'\0')
        {
            const wstring fileName = cursor;
            if (!fileName.empty())
                outFilePaths.push_back((directory / fileName).wstring());

            cursor += fileName.size() + 1;
        }

        return !outFilePaths.empty();
    }

    wstring UI_AnimationSequenceService::Resolve_DefaultAnimationDirectory() const
    {
        vector<fs::path> startPaths = {};

        const fs::path moduleDirectory = Get_ModuleDirectory();
        if (!moduleDirectory.empty())
            startPaths.push_back(moduleDirectory);

        try
        {
            startPaths.push_back(fs::current_path());
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI animation sequence current path query failed: {}", exception.what());
        }

        for (const fs::path& startPath : startPaths)
        {
            fs::path resourceRoot = {};
            if (Try_FindResourceRootFromStart(startPath, resourceRoot))
            {
                const fs::path animationDirectory = (resourceRoot / L"UI" / L"UI_Animations").lexically_normal();
                if (Try_CreateDirectory(animationDirectory))
                    return animationDirectory.wstring();
            }
        }

        const vector<fs::path> fallbackCandidates = {
            L"..\\Bin\\Resources\\UI\\UI_Animations",
            L"..\\Client\\Bin\\Resources\\UI\\UI_Animations",
            L"..\\..\\Client\\Bin\\Resources\\UI\\UI_Animations",
            L"..\\..\\..\\Client\\Bin\\Resources\\UI\\UI_Animations",
            L"Client\\Bin\\Resources\\UI\\UI_Animations",
        };

        for (const fs::path& candidate : fallbackCandidates)
        {
            try
            {
                const fs::path normalizedCandidate = fs::absolute(candidate).lexically_normal();
                if (Try_CreateDirectory(normalizedCandidate))
                    return normalizedCandidate.wstring();
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI animation sequence fallback directory failed: {} ({})", exception.what(), String::ToString(candidate.wstring()));
            }
        }

        return {};
    }

    bool UI_AnimationSequenceService::Add_ClipFromFile(const wstring& filePath, const UI_Document& document)
    {
        UI_AnimationAsset asset = {};
        if (!UI_AnimationAssetSerializer::Load_FromJsonFile(asset, filePath))
            return false;

        UI_AnimationSequenceClip clip = {};
        clip.displayName = Make_DisplayName(asset, filePath);
        clip.filePath = filePath;
        clip.asset = move(asset);
        clip.durationFrames = Calculate_ClipDurationFrames(clip.asset);
        Resolve_TargetObjectsForClip(clip, document);

        _sequence.clips.push_back(move(clip));
        _selectedClipIndex = static_cast<int32>(_sequence.clips.size()) - 1;
        return true;
    }

    int32 UI_AnimationSequenceService::Calculate_ClipDurationFrames(const UI_AnimationAsset& asset) const
    {
        const int32 sourceFps = max(1, asset.fps);
        const int32 sourceFrameCount = max(1, asset.endFrame - asset.startFrame);
        const float durationSeconds = static_cast<float>(sourceFrameCount) / static_cast<float>(sourceFps);
        return max(1, static_cast<int32>(ceilf(durationSeconds * static_cast<float>(max(1, _sequence.fps)))));
    }

    void UI_AnimationSequenceService::Rebuild_Timing()
    {
        int32 frameCursor = 0;
        for (UI_AnimationSequenceClip& clip : _sequence.clips)
        {
            clip.startFrame = frameCursor;
            clip.durationFrames = Calculate_ClipDurationFrames(clip.asset);
            frameCursor += clip.durationFrames;
        }
    }

    void UI_AnimationSequenceService::Clamp_CurrentFrame()
    {
        Set_CurrentFrame(_currentFrame);
    }

    void UI_AnimationSequenceService::Resolve_TargetObjectsForClip(UI_AnimationSequenceClip& clip, const UI_Document& document) const
    {
        clip.resolvedTargetCount = 0;
        for (UI_AnimationTargetBinding& target : clip.asset.targets)
        {
            target.targetObjectId = 0;
            if (target.targetName.empty())
                continue;

            for (const UI_Object& object : document.Get_Objects())
            {
                if (object.name == target.targetName)
                {
                    target.targetObjectId = object.id;
                    ++clip.resolvedTargetCount;
                    break;
                }
            }
        }
    }
}
