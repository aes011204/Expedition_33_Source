#include "UI_RuntimeLoader.h"

#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_Animation.h"
#include "UI_Button.h"
#include "UI_Image.h"
#include "UI_Panel.h"
#include "UI_Text.h"

namespace
{
    // -----------------------------
    // Runtime 내부용 파싱 모델
    // -----------------------------
    enum class RuntimeUIType : uint8
    {
        Image,
        Text,
        Panel,
        Button,
        Unknown,
    };

    struct RuntimeUIObject
    {
        int32 id = 0;
        int32 parentId = 0;
        RuntimeUIType type = RuntimeUIType::Unknown;
        string name{};

        Vec2 position{ 0.f, 0.f };
        Vec2 worldPosition{ 0.f, 0.f };
        Vec2 size{ 100.f, 100.f };
        Vec2 scale{ 1.f, 1.f };
        Vec2 pivot{ 0.5f, 0.5f };
        Vec2 anchorMin{ 0.f, 0.f };
        Vec2 anchorMax{ 0.f, 0.f };
        float rotationRadians = 0.f;
        Vec4 color{ 1.f, 1.f, 1.f, 1.f };
        int32 sortOrder = 0;
        bool visible = true;

        UI_RenderGroup renderGroup = UI_RenderGroup::Foreground;

        string text{};
        float fontSize = 32.f;
        UI_Text::TextAlign textAlign = UI_Text::TextAlign::Left;
        bool textWrap = false;
        string fontTag{};
        string fontGuid{};
        string fontPath{};

        string textureGuid{};
        string texturePath{};
        UI_Image::ImageRenderMode imageRenderMode = UI_Image::ImageRenderMode::Normal;
        string maskTextureGuid{};
        string maskTexturePath{};
        UI_Image::MaskTextureMode maskTextureMode = UI_Image::MaskTextureMode::Alpha;
        string patternMaskTextureGuid{};
        string patternMaskTexturePath{};

        vector<UI_AnimationClip> animationClips{};
        vector<UI_KeyframeAnimationAsset> keyframeAnimationAssets{};
    };

    struct RuntimeUIDocument
    {
        string canvasName = "MainCanvas";
        Vec2 canvasSize{ 1600.f, 900.f };
        vector<RuntimeUIObject> objects{};
    };

    struct RuntimeUIAnimationAssetBinding
    {
        string targetName{};
        UI_KeyframeAnimationAsset asset{};
    };

    // -----------------------------
    // 기본 json 유틸
    // -----------------------------

    string Read_String(const json& item, const char* key, const string& defaultValue = "")
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_string())
            return defaultValue;

        return iter->get<string>();
    }

    int32 Read_Int(const json& item, const char* key, int32 defaultValue = 0)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_number_integer())
            return defaultValue;

        return iter->get<int32>();
    }

    float Read_Float(const json& item, const char* key, float defaultValue = 0.f)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_number())
            return defaultValue;

        return iter->get<float>();
    }

    bool Read_Bool(const json& item, const char* key, bool defaultValue = false)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_boolean())
            return defaultValue;

        return iter->get<bool>();
    }

    Vec2 Read_Vec2(const json& item, const char* key, const Vec2& defaultValue)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_array() || iter->size() < 2)
            return defaultValue;

        return Vec2(
            iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y);
    }

    Vec4 Read_Color(const json& item, const char* key, const Vec4& defaultValue)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_array() || iter->size() < 4)
            return defaultValue;

        Vec4 color(
            iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y,
            iter->at(2).is_number() ? iter->at(2).get<float>() : defaultValue.z,
            iter->at(3).is_number() ? iter->at(3).get<float>() : defaultValue.w);

        // UI_Editor 저장값이 0~255일 수 있으므로 런타임에서 0~1로 정규화한다.
        if (color.x > 1.f || color.y > 1.f || color.z > 1.f || color.w > 1.f)
            color *= 1.f / 255.f;

        return color;
    }

    Vec4 Make_PremultipliedSpriteFontColor(const Vec4& color)
    {
        const float alpha = min(max(color.w, 0.f), 1.f);
        return Vec4(
            min(max(color.x, 0.f), 1.f) * alpha,
            min(max(color.y, 0.f), 1.f) * alpha,
            min(max(color.z, 0.f), 1.f) * alpha,
            alpha);
    }

    bool StartsWith(const string& value, const string& prefix)
    {
        return value.size() >= prefix.size() && equal(prefix.begin(), prefix.end(), value.begin());
    }

    bool EndsWith(const string& value, const string& suffix)
    {
        return value.size() >= suffix.size() && equal(suffix.rbegin(), suffix.rend(), value.rbegin());
    }

    RuntimeUIType Parse_UIType(const string& typeName)
    {
        if ("Image" == typeName)
            return RuntimeUIType::Image;
        if ("Text" == typeName)
            return RuntimeUIType::Text;
        if ("Panel" == typeName)
            return RuntimeUIType::Panel;
        if ("Button" == typeName)
            return RuntimeUIType::Button;

        return RuntimeUIType::Unknown;
    }

    UI_RenderGroup Parse_RenderLayer(const string& layerName)
    {
        const string layer = String::ToLowerCopy(layerName);

        if ("background" == layer)
            return UI_RenderGroup::Background;

        return UI_RenderGroup::Foreground;
    }

    UI_Image::ImageRenderMode Parse_ImageRenderMode(const string& modeName)
    {
        const string mode = String::ToLowerCopy(modeName);

        if ("patternoverlay" == mode || "pattern_overlay" == mode || "pattern overlay" == mode)
            return UI_Image::ImageRenderMode::PatternOverlay;

        return UI_Image::ImageRenderMode::Normal;
    }

    UI_Image::MaskTextureMode Parse_MaskTextureMode(const string& modeName)
    {
        const string mode = String::ToLowerCopy(modeName);

        if ("blackwhite" == mode || "black_white" == mode || "black white" == mode)
            return UI_Image::MaskTextureMode::BlackWhite;

        return UI_Image::MaskTextureMode::Alpha;
    }

    bool Can_RuntimeTypeHaveChildren(RuntimeUIType type)
    {
        return type == RuntimeUIType::Panel || type == RuntimeUIType::Button;
    }

    UI_Text::TextAlign Read_TextAlign(const json& item)
    {
        string align = Read_String(item, "align");
        if (align.empty())
            align = Read_String(item, "textAlign");

        align = String::ToLowerCopy(align);

        if ("center" == align)
            return UI_Text::TextAlign::Center;
        if ("right" == align)
            return UI_Text::TextAlign::Right;

        return UI_Text::TextAlign::Left;
    }

    float Read_RotationRadians(const json& item)
    {
        const auto radiansIter = item.find("rotationRadians");
        if (radiansIter != item.end() && radiansIter->is_number())
            return radiansIter->get<float>();

        return Read_Float(item, "rotation", 0.f);
    }

    bool Try_ReadAnimationTrack(const json& item, UI_AnimationTrack& outTrack)
    {
        if (!item.is_object())
            return false;

        UI_AnimationTrackType trackType = UI_AnimationTrackType::Fade;
        if (!Try_ParseUIAnimationTrackType(Read_String(item, "type", ""), trackType))
            return false;

        UI_AnimationTrack track{};
        track.type = trackType;
        track.enabled = Read_Bool(item, "enabled", true);

        if (track.type == UI_AnimationTrackType::Color)
        {
            track.fromColor = Read_Color(item, "from", Vec4(1.f, 1.f, 1.f, 1.f));
            track.toColor = Read_Color(item, "to", Vec4(1.f, 1.f, 1.f, 1.f));
        }
        else if (track.type == UI_AnimationTrackType::Move || track.type == UI_AnimationTrackType::Scale)
        {
            const Vec2 defaultValue = track.type == UI_AnimationTrackType::Scale ? Vec2(1.f, 1.f) : Vec2(0.f, 0.f);
            track.fromVec2 = Read_Vec2(item, "from", defaultValue);
            track.toVec2 = Read_Vec2(item, "to", defaultValue);
        }
        else
        {
            track.fromFloat = Read_Float(item, "from", 0.f);
            track.toFloat = Read_Float(item, "to", 0.f);

            if (track.type == UI_AnimationTrackType::Fade)
            {
                track.fromFloat = min(max(track.fromFloat, 0.f), 1.f);
                track.toFloat = min(max(track.toFloat, 0.f), 1.f);
            }
        }

        outTrack = track;
        return true;
    }

    bool Try_ReadAnimationClip(const json& item, UI_AnimationClip& outClip)
    {
        if (!item.is_object())
            return false;

        UI_AnimationClip clip{};
        clip.name = String::ToWString(Read_String(item, "name", "Default"));
        clip.enabled = Read_Bool(item, "enabled", true);
        clip.playOnOpen = Read_Bool(item, "playOnOpen", true);
        clip.delay = max(0.f, Read_Float(item, "delay", 0.f));
        clip.duration = max(0.001f, Read_Float(item, "duration", 0.35f));
        clip.loop = Read_Bool(item, "loop", false);

        UI_AnimationEasing easing = UI_AnimationEasing::EaseOut;
        const string easingName = Read_String(item, "easing", "EaseOut");
        if (Try_ParseUIAnimationEasing(easingName, easing))
            clip.easing = easing;
        else
            LOG_WARN("UI RuntimeLoader unknown animation easing '{}'. Using EaseOut.", easingName);

        const auto tracksIter = item.find("tracks");
        if (tracksIter != item.end() && tracksIter->is_array())
        {
            for (const json& trackItem : *tracksIter)
            {
                UI_AnimationTrack track{};
                if (!Try_ReadAnimationTrack(trackItem, track))
                {
                    LOG_WARN("UI RuntimeLoader skipped invalid animation track.");
                    continue;
                }

                clip.tracks.push_back(track);
            }
        }

        outClip = move(clip);
        return true;
    }

    vector<UI_AnimationClip> Read_AnimationClips(const json& item)
    {
        vector<UI_AnimationClip> clips{};

        const auto animationsIter = item.find("animations");
        if (animationsIter == item.end() || !animationsIter->is_array())
            return clips;

        for (const json& animationItem : *animationsIter)
        {
            UI_AnimationClip clip{};
            if (!Try_ReadAnimationClip(animationItem, clip))
            {
                LOG_WARN("UI RuntimeLoader skipped invalid animation clip.");
                continue;
            }

            clips.push_back(move(clip));
        }

        return clips;
    }

    vector<UI_AnimationClip> Make_TextAnimationClips(const RuntimeUIObject& object)
    {
        vector<UI_AnimationClip> clips = object.animationClips;
        const float fontScale = max(4.f, object.fontSize) / 32.f;

        for (UI_AnimationClip& clip : clips)
        {
            for (UI_AnimationTrack& track : clip.tracks)
            {
                if (track.type != UI_AnimationTrackType::Scale)
                    continue;

                track.fromVec2.x *= fontScale;
                track.toVec2.x *= fontScale;
            }
        }

        return clips;
    }

    void Sort_KeyframeTrack(UI_KeyframeAnimationTrack& track, const UI_KeyframeAnimationAsset& asset)
    {
        for (UI_KeyframeAnimationKey& key : track.keys)
        {
            key.frame = min(max(key.frame, asset.startFrame), asset.endFrame);
            if (track.type == UI_KeyframeAnimationTrackType::ScaleX || track.type == UI_KeyframeAnimationTrackType::ScaleY)
                key.value = max(0.01f, key.value);
            else if (track.type == UI_KeyframeAnimationTrackType::Alpha)
                key.value = min(max(key.value, 0.f), 1.f);
        }

        stable_sort(
            track.keys.begin(),
            track.keys.end(),
            [](const UI_KeyframeAnimationKey& lhs, const UI_KeyframeAnimationKey& rhs)
            {
                return lhs.frame < rhs.frame;
            });

        vector<UI_KeyframeAnimationKey> mergedKeys;
        mergedKeys.reserve(track.keys.size());
        for (const UI_KeyframeAnimationKey& key : track.keys)
        {
            if (!mergedKeys.empty() && mergedKeys.back().frame == key.frame)
            {
                mergedKeys.back() = key;
                continue;
            }

            mergedKeys.push_back(key);
        }

        track.keys = move(mergedKeys);
    }

    bool Try_ReadKeyframeAnimationKey(const json& item, UI_KeyframeAnimationKey& outKey)
    {
        if (!item.is_object())
            return false;

        UI_KeyframeAnimationKey key{};
        key.frame = Read_Int(item, "frame", 0);
        key.value = Read_Float(item, "value", 0.f);
        key.easing = outKey.easing;

        UI_AnimationEasing easing = key.easing;
        const string easingName = Read_String(item, "easing", "");
        if (!easingName.empty() && Try_ParseUIAnimationEasing(easingName, easing))
            key.easing = easing;

        outKey = key;
        return true;
    }

    bool Try_ReadKeyframeAnimationTrack(const json& item, UI_KeyframeAnimationTrack& outTrack)
    {
        if (!item.is_object())
            return false;

        UI_KeyframeAnimationTrackType trackType = UI_KeyframeAnimationTrackType::PositionX;
        const string typeName = Read_String(item, "type", "");
        if (!Try_ParseUIKeyframeAnimationTrackType(typeName, trackType))
        {
            LOG_WARN("UI RuntimeLoader unknown keyframe animation track type '{}'.", typeName);
            return false;
        }

        UI_KeyframeAnimationTrack track{};
        track.type = trackType;
        track.enabled = Read_Bool(item, "enabled", true);

        UI_AnimationEasing easing = UI_AnimationEasing::Linear;
        if (Try_ParseUIAnimationEasing(Read_String(item, "easing", "Linear"), easing))
            track.easing = easing;

        const auto keysIter = item.find("keys");
        if (keysIter != item.end() && keysIter->is_array())
        {
            for (const json& keyItem : *keysIter)
            {
                UI_KeyframeAnimationKey key{};
                key.easing = track.easing;
                if (Try_ReadKeyframeAnimationKey(keyItem, key))
                    track.keys.push_back(key);
            }
        }

        outTrack = move(track);
        return true;
    }

    bool Try_ReadKeyframeAnimationAssetBinding(const json& root, RuntimeUIAnimationAssetBinding& outBinding)
    {
        if (!root.is_object())
            return false;

        RuntimeUIAnimationAssetBinding binding{};
        binding.targetName = Read_String(root, "targetName", "");
        if (binding.targetName.empty())
            return false;

        UI_KeyframeAnimationAsset asset{};
        asset.name = String::ToWString(Read_String(root, "name", "NewUIAnimation"));
        asset.enabled = true;
        asset.playOnOpen = true;
        asset.fps = max(1, Read_Int(root, "fps", 60));
        asset.startFrame = Read_Int(root, "startFrame", 0);
        asset.endFrame = max(asset.startFrame + 1, Read_Int(root, "endFrame", 60));
        asset.loop = Read_Bool(root, "loop", false);

        const auto tracksIter = root.find("tracks");
        if (tracksIter != root.end() && tracksIter->is_array())
        {
            for (const json& trackItem : *tracksIter)
            {
                UI_KeyframeAnimationTrack track{};
                if (!Try_ReadKeyframeAnimationTrack(trackItem, track))
                    continue;

                Sort_KeyframeTrack(track, asset);
                asset.tracks.push_back(move(track));
            }
        }

        binding.asset = move(asset);
        outBinding = move(binding);
        return true;
    }

    bool Load_KeyframeAnimationAssetFile(const fs::path& assetPath, RuntimeUIAnimationAssetBinding& outBinding)
    {
        try
        {
            ifstream file(assetPath);
            if (!file.is_open())
                return false;

            json root = json::parse(file, nullptr, false);
            if (root.is_discarded())
                return false;

            return Try_ReadKeyframeAnimationAssetBinding(root, outBinding);
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI RuntimeLoader keyframe animation parse failed: {} ({})",
                exception.what(),
                String::ToString(assetPath.wstring()));
            return false;
        }
    }

    vector<RuntimeUIAnimationAssetBinding> Load_KeyframeAnimationAssetBindings(const wstring& animationDirectory)
    {
        vector<RuntimeUIAnimationAssetBinding> bindings{};
        if (animationDirectory.empty())
            return bindings;

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
            return bindings;

        const fs::path directoryPath = (fs::path(assetRoot) / animationDirectory).lexically_normal();
        if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath))
        {
            LOG_WARN("UI RuntimeLoader animation directory not found: {}", String::ToString(directoryPath.wstring()));
            return bindings;
        }

        try
        {
            for (const fs::directory_entry& entry : fs::directory_iterator(directoryPath))
            {
                if (!entry.is_regular_file())
                    continue;

                const string fileName = String::ToLowerCopy(String::ToString(entry.path().filename().wstring()));
                if (!EndsWith(fileName, ".uianim.json"))
                    continue;

                RuntimeUIAnimationAssetBinding binding{};
                if (Load_KeyframeAnimationAssetFile(entry.path(), binding))
                    bindings.push_back(move(binding));
            }
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI RuntimeLoader animation directory scan failed: {} ({})",
                exception.what(),
                String::ToString(directoryPath.wstring()));
        }

        return bindings;
    }

    vector<UI_KeyframeAnimationAsset> Make_TextKeyframeAnimationAssets(const RuntimeUIObject& object)
    {
        vector<UI_KeyframeAnimationAsset> assets = object.keyframeAnimationAssets;
        const float fontScale = max(4.f, object.fontSize) / 32.f;

        for (UI_KeyframeAnimationAsset& asset : assets)
        {
            for (UI_KeyframeAnimationTrack& track : asset.tracks)
            {
                if (track.type != UI_KeyframeAnimationTrackType::ScaleX)
                    continue;

                for (UI_KeyframeAnimationKey& key : track.keys)
                    key.value *= fontScale;
            }
        }

        return assets;
    }

    wstring Resolve_RuntimeResourcePath(const string& runtimePath)
    {
        if (runtimePath.empty())
            return {};

        string normalizedPath = runtimePath;
        replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

        const fs::path inputPath = fs::path(String::ToWString(normalizedPath));
        if (inputPath.is_absolute())
            return inputPath.lexically_normal().wstring();

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
            return fs::absolute(inputPath).lexically_normal().wstring();

        const string loweredPath = String::ToLowerCopy(normalizedPath);
        string resourceRelativePath = normalizedPath;

        constexpr const char* resourcePrefix = "../bin/resources/";
        constexpr const char* binResourcePrefix = "bin/resources/";
        constexpr const char* resourcesPrefix = "resources/";

        if (StartsWith(loweredPath, resourcePrefix))
            resourceRelativePath = normalizedPath.substr(strlen(resourcePrefix));
        else if (StartsWith(loweredPath, binResourcePrefix))
            resourceRelativePath = normalizedPath.substr(strlen(binResourcePrefix));
        else if (StartsWith(loweredPath, resourcesPrefix))
            resourceRelativePath = normalizedPath.substr(strlen(resourcesPrefix));

        return (fs::path(assetRoot) / fs::path(String::ToWString(resourceRelativePath))).lexically_normal().wstring();
    }

    wstring Resolve_JsonAssetPath(const string& guid, const string& path)
    {
        // guid가 있으면 guid를 우선 신뢰한다.
        if (!guid.empty())
        {
            const wstring guidPath = GAME->Resolve_AssetPath(guid);
            if (!guidPath.empty() && fs::exists(guidPath))
                return guidPath;
        }

        // guid 해석이 안 되면 저장된 경로를 fallback으로 사용한다.
        return Resolve_RuntimeResourcePath(path);
    }

    bool Ensure_FontLoaded(const wstring& fontTag, const wstring& fontPath, unordered_set<wstring>& loadedFontTags)
    {
        if (fontTag.empty() || fontPath.empty())
            return false;

        // 같은 로더 실행 중 같은 tag를 가진 폰트는 한 번만 등록한다.
        if (loadedFontTags.find(fontTag) != loadedFontTags.end())
            return true;

        if (GAME->Has_Font(fontTag))
        {
            loadedFontTags.insert(fontTag);
            return true;
        }

        if (FAILED(GAME->Add_Font(fontTag, fontPath.c_str())))
        {
            LOG_ERROR("UI RuntimeLoader Add_Font failed. tag={}, path={}",
                String::ToString(fontTag),
                String::ToString(fontPath));

            return false;
        }

        loadedFontTags.insert(fontTag);
        return true;
    }

    bool Load_JsonRoot(const wstring& jsonPath, json& outRoot)
    {
        if (jsonPath.empty())
        {
            LOG_ERROR("UI RuntimeLoader load failed: empty json path");
            return false;
        }

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
        {
            LOG_ERROR("UI RuntimeLoader load failed: asset root is empty");
            return false;
        }

        const fs::path fullPath = (fs::path(assetRoot) / jsonPath).lexically_normal();
        if (!fs::exists(fullPath))
        {
            LOG_WARN("UI RuntimeLoader json not found: {}", String::ToString(fullPath.wstring()));
            return false;
        }

        try
        {
            ifstream file(fullPath);
            if (!file.is_open())
            {
                LOG_ERROR("UI RuntimeLoader failed to open json: {}", String::ToString(fullPath.wstring()));
                return false;
            }

            file >> outRoot;
            return true;
        }
        catch (const exception& exception)
        {
            LOG_ERROR("UI RuntimeLoader json parse failed: {} ({})",
                exception.what(),
                String::ToString(fullPath.wstring()));
            return false;
        }
    }

    void Parse_Canvas(const json& root, RuntimeUIDocument& outDocument)
    {
        const auto canvasIter = root.find("canvas");
        if (canvasIter == root.end() || !canvasIter->is_object())
            return;

        outDocument.canvasName = Read_String(*canvasIter, "name", "MainCanvas");
        outDocument.canvasSize = Vec2(
            max(1.f, Read_Float(*canvasIter, "width", 1600.f)),
            max(1.f, Read_Float(*canvasIter, "height", 900.f)));
    }

    RuntimeUIObject Parse_Object(const json& item)
    {
        RuntimeUIObject object{};

        object.id = Read_Int(item, "id", 0);
        object.parentId = Read_Int(item, "parentId", 0);
        object.name = Read_String(item, "name", "");
        object.type = Parse_UIType(Read_String(item, "type", ""));

        object.position = Read_Vec2(item, "position", Vec2(0.f, 0.f));
        object.size = Read_Vec2(item, "size", Vec2(100.f, 100.f));
        object.scale = Read_Vec2(item, "scale", Vec2(1.f, 1.f));
        object.pivot = Read_Vec2(item, "pivot", Vec2(0.5f, 0.5f));
        object.anchorMin = Read_Vec2(item, "anchorMin", Vec2(0.f, 0.f));
        object.anchorMax = Read_Vec2(item, "anchorMax", Vec2(0.f, 0.f));
        object.rotationRadians = Read_RotationRadians(item);
        object.color = Read_Color(item, "color", Vec4(1.f, 1.f, 1.f, 1.f));
        object.sortOrder = Read_Int(item, "sortOrder", 0);
        object.visible = Read_Bool(item, "visible", true);
        object.renderGroup = Parse_RenderLayer(Read_String(item, "renderLayer", "Foreground"));

        object.text = Read_String(item, "text", "");
        object.fontSize = max(4.f, Read_Float(item, "fontSize", 32.f));
        object.textAlign = Read_TextAlign(item);
        object.textWrap = Read_Bool(item, "textWrap", Read_Bool(item, "wrap", false));
        object.fontTag = Read_String(item, "fontTag", "");
        object.fontGuid = Read_String(item, "fontGuid", "");
        object.fontPath = Read_String(item, "fontPath", "");

        object.textureGuid = Read_String(item, "textureGuid", "");
        object.texturePath = Read_String(item, "texturePath", "");
        object.imageRenderMode = Parse_ImageRenderMode(Read_String(item, "imageRenderMode", "Normal"));
        object.maskTextureGuid = Read_String(item, "maskTextureGuid", "");
        object.maskTexturePath = Read_String(item, "maskTexturePath", "");
        object.maskTextureMode = Parse_MaskTextureMode(Read_String(item, "maskTextureMode", "Alpha"));
        object.patternMaskTextureGuid = Read_String(item, "patternMaskTextureGuid", "");
        object.patternMaskTexturePath = Read_String(item, "patternMaskTexturePath", "");
        object.animationClips = Read_AnimationClips(item);

        return object;
    }

    Vec2 Resolve_WorldPosition(RuntimeUIDocument& document, RuntimeUIObject& object, unordered_set<int32>& resolvingIds)
    {
        if (object.parentId == 0)
        {
            object.worldPosition = object.position;
            return object.worldPosition;
        }

        if (object.parentId == object.id || resolvingIds.find(object.id) != resolvingIds.end())
        {
            LOG_WARN("UI RuntimeLoader object {} has cyclic parent. Using canvas space.", object.id);
            object.parentId = 0;
            object.worldPosition = object.position;
            return object.worldPosition;
        }

        const int32 originalParentId = object.parentId;
        RuntimeUIObject* parentObject = nullptr;
        for (RuntimeUIObject& candidate : document.objects)
        {
            if (candidate.id == originalParentId)
            {
                parentObject = &candidate;
                break;
            }
        }

        if (nullptr == parentObject)
        {
            LOG_WARN("UI RuntimeLoader object {} has missing parent {}. Using canvas space.", object.id, object.parentId);
            object.parentId = 0;
            object.worldPosition = object.position;
            return object.worldPosition;
        }

        resolvingIds.insert(object.id);
        const Vec2 parentWorldPosition = Resolve_WorldPosition(document, *parentObject, resolvingIds);
        resolvingIds.erase(object.id);

        if (object.parentId != originalParentId)
        {
            object.worldPosition = object.position;
            return object.worldPosition;
        }

        object.worldPosition = Vec2(parentWorldPosition.x + object.position.x, parentWorldPosition.y + object.position.y);
        return object.worldPosition;
    }

    void Resolve_WorldPositions(RuntimeUIDocument& document)
    {
        unordered_set<int32> resolvingIds;

        for (RuntimeUIObject& object : document.objects)
        {
            resolvingIds.clear();
            Resolve_WorldPosition(document, object, resolvingIds);
        }
    }

    bool Parse_Document(const json& root, RuntimeUIDocument& outDocument)
    {
        if (!root.is_object())
        {
            LOG_ERROR("UI RuntimeLoader root is not object");
            return false;
        }

        Parse_Canvas(root, outDocument);

        const auto objectsIter = root.find("objects");
        if (objectsIter == root.end() || !objectsIter->is_array())
        {
            LOG_WARN("UI RuntimeLoader json has no objects array");
            return true;
        }

        outDocument.objects.clear();
        outDocument.objects.reserve(objectsIter->size());

        for (const json& item : *objectsIter)
        {
            if (!item.is_object())
                continue;

            outDocument.objects.push_back(Parse_Object(item));
        }

        stable_sort(outDocument.objects.begin(), outDocument.objects.end(),
            [](const RuntimeUIObject& lhs, const RuntimeUIObject& rhs)
            {
                if (lhs.sortOrder != rhs.sortOrder)
                    return lhs.sortOrder < rhs.sortOrder;

                return lhs.id < rhs.id;
            });

        Resolve_WorldPositions(outDocument);

        return true;
    }

    void Apply_KeyframeAnimationAssets(
        RuntimeUIDocument& document,
        const vector<RuntimeUIAnimationAssetBinding>& animationBindings)
    {
        if (animationBindings.empty())
            return;

        for (const RuntimeUIAnimationAssetBinding& binding : animationBindings)
        {
            bool bound = false;
            for (RuntimeUIObject& object : document.objects)
            {
                if (object.name != binding.targetName)
                    continue;

                object.keyframeAnimationAssets.push_back(binding.asset);
                bound = true;
            }

            if (!bound)
                LOG_WARN("UI RuntimeLoader keyframe animation target not found: {}", binding.targetName);
        }
    }

    HRESULT Spawn_Image(
        uint32 levelIndex,
        const wstring& prototypeTagPrefix,
        const RuntimeUIObject& object,
        Shared<UI_Base>* outUI)
    {
        const wstring texturePath = Resolve_JsonAssetPath(object.textureGuid, object.texturePath);
        if (texturePath.empty() || !fs::exists(texturePath))
        {
            LOG_WARN("UI RuntimeLoader image skipped: missing texture. id={}", object.id);
            return S_OK;
        }

        UI_Image::UI_IMAGE_DESC desc{};
        desc.texturePath = texturePath;
        desc.imageRenderMode = object.imageRenderMode;
        desc.maskTextureMode = object.maskTextureMode;

        const wstring maskTexturePath = Resolve_JsonAssetPath(object.maskTextureGuid, object.maskTexturePath);
        if (!maskTexturePath.empty() && fs::exists(maskTexturePath))
            desc.maskTexturePath = maskTexturePath;
        else if (!object.maskTextureGuid.empty() || !object.maskTexturePath.empty())
            LOG_WARN("UI RuntimeLoader image mask skipped: missing mask texture. id={}", object.id);

        const wstring patternMaskTexturePath = Resolve_JsonAssetPath(object.patternMaskTextureGuid, object.patternMaskTexturePath);
        if (!patternMaskTexturePath.empty() && fs::exists(patternMaskTexturePath))
            desc.patternMaskTexturePath = patternMaskTexturePath;
        else if (!object.patternMaskTextureGuid.empty() || !object.patternMaskTexturePath.empty())
            LOG_WARN("UI RuntimeLoader image pattern mask skipped: missing pattern mask texture. id={}", object.id);

        desc.color = object.color;
        desc.transformDesc.position = object.position;
        desc.transformDesc.size = object.size;
        desc.transformDesc.scale = object.scale;
        desc.transformDesc.rotationRadians = object.rotationRadians;
        desc.transformDesc.pivot = object.pivot;
        desc.transformDesc.anchorMin = object.anchorMin;
        desc.transformDesc.anchorMax = object.anchorMax;
        desc.sortOrder = object.sortOrder;
        desc.isOpenOnCreate = object.visible;
        desc.animationClips = object.animationClips;
        desc.keyframeAnimationAssets = object.keyframeAnimationAssets;
        desc.renderGroup = object.renderGroup;

        // object id 기반 prototype tag를 유지해 기존 동작을 최대한 그대로 둔다.
        const wstring prototypeTag =
            prototypeTagPrefix + L"_Image_" + to_wstring(object.id);

        if (!GAME->Has_Prototype(levelIndex, prototypeTag))
        {
            if (FAILED(GAME->Add_Prototype(levelIndex, prototypeTag, UI_Image::Create())))
                return E_FAIL;
        }

        if (FAILED(GAME->Add_UI(levelIndex, prototypeTag, &desc, outUI)))
            return E_FAIL;

        return S_OK;
    }

    HRESULT Spawn_Text(
        uint32 levelIndex,
        const wstring& prototypeTagPrefix,
        const RuntimeUIObject& object,
        unordered_set<wstring>& loadedFontTags,
        Shared<UI_Base>* outUI)
    {
        const wstring fontPath = Resolve_JsonAssetPath(object.fontGuid, object.fontPath);
        if (fontPath.empty() || !fs::exists(fontPath))
        {
            LOG_WARN("UI RuntimeLoader text skipped: missing font. id={}", object.id);
            return S_OK;
        }

        // 저장된 tag가 있으면 우선 사용하고, 없으면 파일명 stem을 tag로 만든다.
        const wstring fontTag = object.fontTag.empty()
            ? fs::path(fontPath).stem().wstring()
            : String::ToWString(object.fontTag);

        if (!Ensure_FontLoaded(fontTag, fontPath, loadedFontTags))
            return E_FAIL;

        Vec2 scale = object.scale;
        scale.x *= object.fontSize / 32.f;

        UI_Text::UI_TEXT_DESC desc{};
        desc.text = String::ToWString(object.text);
        desc.fontTag = fontTag;
        desc.color = Make_PremultipliedSpriteFontColor(object.color);
        desc.textAlign = object.textAlign;
        desc.textWrap = object.textWrap;
        desc.transformDesc.position = object.position;
        desc.transformDesc.size = object.size;
        desc.transformDesc.scale = scale;
        desc.transformDesc.pivot = object.pivot;
        desc.transformDesc.anchorMin = object.anchorMin;
        desc.transformDesc.anchorMax = object.anchorMax;
        desc.transformDesc.rotationRadians = object.rotationRadians;
        desc.sortOrder = object.sortOrder;
        desc.isOpenOnCreate = object.visible;
        desc.animationClips = Make_TextAnimationClips(object);
        desc.keyframeAnimationAssets = Make_TextKeyframeAnimationAssets(object);
        desc.renderGroup = object.renderGroup;

        const wstring prototypeTag =
            prototypeTagPrefix + L"_Text_" + to_wstring(object.id);

        if (!GAME->Has_Prototype(levelIndex, prototypeTag))
        {
            if (FAILED(GAME->Add_Prototype(levelIndex, prototypeTag, UI_Text::Create())))
                return E_FAIL;
        }

        if (FAILED(GAME->Add_UI(levelIndex, prototypeTag, &desc, outUI)))
            return E_FAIL;

        return S_OK;
    }

    HRESULT Spawn_Panel(
        uint32 levelIndex,
        const wstring& prototypeTagPrefix,
        const RuntimeUIObject& object,
        Shared<UI_Base>* outUI)
    {
        UI_Panel::UI_PANEL_DESC desc{};
        desc.color = object.color;
        desc.transformDesc.position = object.position;
        desc.transformDesc.size = object.size;
        desc.transformDesc.scale = object.scale;
        desc.transformDesc.pivot = object.pivot;
        desc.transformDesc.anchorMin = object.anchorMin;
        desc.transformDesc.anchorMax = object.anchorMax;
        desc.transformDesc.rotationRadians = object.rotationRadians;
        desc.sortOrder = object.sortOrder;
        desc.isOpenOnCreate = object.visible;
        desc.animationClips = object.animationClips;
        desc.keyframeAnimationAssets = object.keyframeAnimationAssets;
        desc.renderGroup = object.renderGroup;

        const wstring prototypeTag =
            prototypeTagPrefix + L"_Panel_" + to_wstring(object.id);

        if (!GAME->Has_Prototype(levelIndex, prototypeTag))
        {
            if (FAILED(GAME->Add_Prototype(levelIndex, prototypeTag, UI_Panel::Create())))
                return E_FAIL;
        }

        if (FAILED(GAME->Add_UI(levelIndex, prototypeTag, &desc, outUI)))
            return E_FAIL;

        return S_OK;
    }

    HRESULT Spawn_Button(
    uint32 levelIndex,
    const wstring& prototypeTagPrefix,
    const RuntimeUIObject& object,
    Shared<UI_Base>* outUI)
    {
        UI_Button::UI_BUTTON_DESC desc{};
        desc.color = object.color;
        desc.transformDesc.position = object.position;
        desc.transformDesc.size = object.size;
        desc.transformDesc.scale = object.scale;
        desc.transformDesc.pivot = object.pivot;
        desc.transformDesc.anchorMin = object.anchorMin;
        desc.transformDesc.anchorMax = object.anchorMax;
        desc.transformDesc.rotationRadians = object.rotationRadians;
        desc.sortOrder = object.sortOrder;
        desc.isOpenOnCreate = object.visible;
        desc.animationClips = object.animationClips;
        desc.keyframeAnimationAssets = object.keyframeAnimationAssets;
        desc.renderGroup = object.renderGroup;

        const wstring prototypeTag =
            prototypeTagPrefix + L"_Button_" + to_wstring(object.id);

        if (!GAME->Has_Prototype(levelIndex, prototypeTag))
        {
            if (FAILED(GAME->Add_Prototype(levelIndex, prototypeTag, UI_Button::Create())))
                return E_FAIL;
        }

        if (FAILED(GAME->Add_UI(levelIndex, prototypeTag, &desc, outUI)))
            return E_FAIL;

        return S_OK;
    }

    HRESULT Spawn_Object(
        uint32 levelIndex,
        const wstring& prototypeTagPrefix,
        const RuntimeUIObject& object,
        unordered_set<wstring>& loadedFontTags,
        Shared<UI_Base>* outUI)
    {
        if (nullptr != outUI)
            outUI->reset();

        switch (object.type)
        {
        case RuntimeUIType::Image:
            return Spawn_Image(levelIndex, prototypeTagPrefix, object, outUI);

        case RuntimeUIType::Text:
            return Spawn_Text(levelIndex, prototypeTagPrefix, object, loadedFontTags, outUI);

        case RuntimeUIType::Panel:
            return Spawn_Panel(levelIndex, prototypeTagPrefix, object, outUI);

        case RuntimeUIType::Button:
            return Spawn_Button(levelIndex, prototypeTagPrefix, object, outUI);

        case RuntimeUIType::Unknown:
        default:
            LOG_WARN("UI RuntimeLoader unknown type skipped. id={}, name={}", object.id, object.name);
            return S_OK;
        }
    }

    bool Has_ParentCycle(const unordered_map<int32, const RuntimeUIObject*>& objectsById, int32 objectId, int32 parentId)
    {
        unordered_set<int32> visitedIds;
        int32 currentParentId = parentId;

        while (currentParentId != 0)
        {
            if (currentParentId == objectId)
                return true;

            if (!visitedIds.insert(currentParentId).second)
                return true;

            const auto parentIter = objectsById.find(currentParentId);
            if (parentIter == objectsById.end() || nullptr == parentIter->second)
                return false;

            currentParentId = parentIter->second->parentId;
        }

        return false;
    }

    void Link_ParentRelations(const RuntimeUIDocument& document, const unordered_map<int32, Shared<UI_Base>>& spawnedUIById)
    {
        unordered_map<int32, const RuntimeUIObject*> objectsById;
        objectsById.reserve(document.objects.size());

        for (const RuntimeUIObject& object : document.objects)
        {
            if (object.id == 0)
                continue;

            objectsById.emplace(object.id, &object);
        }

        for (const RuntimeUIObject& object : document.objects)
        {
            if (object.parentId == 0)
                continue;

            const auto childUIIter = spawnedUIById.find(object.id);
            if (childUIIter == spawnedUIById.end() || nullptr == childUIIter->second)
                continue;

            const auto parentObjectIter = objectsById.find(object.parentId);
            if (parentObjectIter == objectsById.end() || nullptr == parentObjectIter->second)
            {
                LOG_WARN("UI RuntimeLoader object {} has missing parent {}. Using canvas space.", object.id, object.parentId);
                childUIIter->second->Clear_Parent();
                continue;
            }

            if (!Can_RuntimeTypeHaveChildren(parentObjectIter->second->type))
            {
                LOG_WARN("UI RuntimeLoader object {} has non-container parent {}. Using canvas space.", object.id, object.parentId);
                childUIIter->second->Clear_Parent();
                continue;
            }

            if (object.id == object.parentId || Has_ParentCycle(objectsById, object.id, object.parentId))
            {
                LOG_WARN("UI RuntimeLoader object {} has cyclic parent chain. Using canvas space.", object.id);
                childUIIter->second->Clear_Parent();
                continue;
            }

            const auto parentUIIter = spawnedUIById.find(object.parentId);
            if (parentUIIter == spawnedUIById.end() || nullptr == parentUIIter->second)
            {
                LOG_WARN("UI RuntimeLoader object {} parent {} was not spawned. Using canvas space.", object.id, object.parentId);
                childUIIter->second->Clear_Parent();
                continue;
            }

            childUIIter->second->Set_Parent(parentUIIter->second);
        }
    }
}

HRESULT UI_RuntimeLoader::Load_Document(const UI_LoadDesc& desc)
{
    wstring prototypeTagPrefix = desc.prototypeTagPrefix;
    if (prototypeTagPrefix.empty())
        prototypeTagPrefix = fs::path(desc.jsonPath).stem().wstring();

    if (prototypeTagPrefix.empty())
        prototypeTagPrefix = L"UIJson";

    if (nullptr != desc.outUIByName)
        desc.outUIByName->clear();

    if (nullptr != desc.outChildrenByParentName)
        desc.outChildrenByParentName->clear();

    json root{};
    if (!Load_JsonRoot(desc.jsonPath, root))
        return E_FAIL;

    RuntimeUIDocument document{};
    if (!Parse_Document(root, document))
        return E_FAIL;

    Apply_KeyframeAnimationAssets(document, Load_KeyframeAnimationAssetBindings(desc.animationDirectory));

    unordered_set<wstring> loadedFontTags;
    unordered_map<int32, Shared<UI_Base>> spawnedUIById;

    for (const RuntimeUIObject& object : document.objects)
    {
        Shared<UI_Base> spawnedUI = nullptr;
        const HRESULT hr = Spawn_Object(desc.levelIndex, prototypeTagPrefix, object, loadedFontTags, &spawnedUI);
        if (FAILED(hr))
            return hr;

        if (nullptr == spawnedUI || object.id == 0)
            continue;

        if (!object.name.empty())
        {
            const wstring objectName = String::ToWString(object.name);
            spawnedUI->Set_Name(objectName);

            if (nullptr != desc.outUIByName)
            {
                const bool passesNameFilter =
                    desc.outUINamePrefixFilter.empty() ||
                    objectName.rfind(desc.outUINamePrefixFilter, 0) == 0;

                if (passesNameFilter)
                {
                    auto [nameIter, inserted] = desc.outUIByName->emplace(objectName, spawnedUI);
                    if (!inserted)
                        nameIter->second = spawnedUI;
                }
            }
        }

        if (!spawnedUIById.emplace(object.id, spawnedUI).second)
            LOG_WARN("UI RuntimeLoader duplicated spawned id skipped in parent map: {}", object.id);
    }

    Link_ParentRelations(document, spawnedUIById);

    if (nullptr != desc.outChildrenByParentName)
    {
        unordered_map<int32, const RuntimeUIObject*> objectsById;
        objectsById.reserve(document.objects.size());

        for (const RuntimeUIObject& object : document.objects)
        {
            if (object.id == 0)
                continue;

            objectsById.emplace(object.id, &object);
        }

        for (const RuntimeUIObject& object : document.objects)
        {
            if (object.parentId == 0)
                continue;

            const auto childIter = spawnedUIById.find(object.id);
            if (childIter == spawnedUIById.end() || childIter->second == nullptr)
                continue;

            const auto parentObjectIter = objectsById.find(object.parentId);
            if (parentObjectIter == objectsById.end() || parentObjectIter->second == nullptr)
                continue;

            const string& parentNameUtf8 = parentObjectIter->second->name;
            if (parentNameUtf8.empty())
                continue;

            const wstring parentName = String::ToWString(parentNameUtf8);
            desc.outChildrenByParentName->operator[](parentName).push_back(childIter->second);
        }
    }


    return S_OK;
}
