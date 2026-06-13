#include "pch.h"
#include "EffectAssetRuntimeLoader.h"

#include "EffectAssetRuntimeLoader_Support.h"
#include "GameInstance.h"

NS_BEGIN(Client)

using namespace EffectAssetRuntimeLoad::Assets;
using namespace EffectAssetRuntimeLoad::Emitters;
using namespace EffectAssetRuntimeLoad::Json;

Shared<const EffectDefinition> EffectAssetRuntimeLoader::Load_Definition(const wstring& effectAssetPath)
{
    json root{};
    if (!Load_JsonRoot(effectAssetPath, root))
        return nullptr;

    if (!root.is_object())
    {
        LOG_ERROR("Root is not object. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    const auto emittersIter = root.find("emitters");
    if (emittersIter == root.end() || !emittersIter->is_array())
    {
        LOG_ERROR("Emitters array is missing. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    auto definition = make_shared<EffectDefinition>();
    definition->name = Read_String(root, "name");
    if (definition->name.empty())
        definition->name = String::ToString(fs::path(effectAssetPath).stem().wstring());

    for (const json& emitter : *emittersIter)
    {
        if (!emitter.is_object())
        {
            LOG_WARN("Skipped invalid emitter node. effect='{}'", definition->name);
            continue;
        }

        if (!Read_Bool(emitter, "enabled", true))
            continue;

        const string rendererType = Resolve_RendererType(emitter);
        const string rendererTypeLower = String::ToLowerCopy(rendererType);

        EffectEmitterDefinition emitterDefinition{};
        bool converted = false;

        if (rendererTypeLower == "sprite")
            converted = Build_EmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "trail")
            converted = Build_TrailEmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "sourcehistoryribbon" || rendererTypeLower == "ribbon")
            converted = Build_SourceHistoryRibbonEmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "sourcehistoryspritetrail")
            converted = Build_SourceHistorySpriteTrailEmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "beam")
            converted = Build_BeamEmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "mesh")
            converted = Build_MeshEmitterDefinition(emitter, emitterDefinition);
        else
        {
            LOG_INFO(
                "Skipped unsupported renderer. effect='{}', emitter='{}', rendererType='{}'",
                definition->name,
                Read_String(emitter, "name"),
                rendererType
            );
            continue;
        }

        if (!converted)
        {
            LOG_WARN(
                "EffectAssetRuntimeLoader skipped emitter conversion failure. effect='{}', emitter='{}'",
                definition->name,
                Read_String(emitter, "name")
            );
            continue;
        }

        definition->emitters.push_back(emitterDefinition);
    }

    if (definition->emitters.empty())
    {
        LOG_ERROR("No runtime emitter converted. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    return definition;
}

Shared<const EffectDefinition> EffectAssetRuntimeLoader::Load_DefinitionByName(const wstring& effectName)
{
    string effectAssetGuid{};
    if (!Find_EffectAssetGuidByName(effectName, effectAssetGuid))
        return nullptr;

    wstring resolvedEffectAssetPath{};
    if (!Resolve_EffectAssetPathByGuid(effectAssetGuid, resolvedEffectAssetPath))
        return nullptr;

    return Load_Definition(resolvedEffectAssetPath);
}

namespace EffectAssetRuntimeLoad::Assets
{

bool Try_MakeResourceRelativePath(
    const fs::path& fullPath,
    const char* logContext,
    const string& assetKey,
    wstring& outRelativePath)
{
    outRelativePath.clear();

    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
    {
        LOG_ERROR(
            "EffectAssetRuntimeLoader {} failed: asset root is empty. key='{}'",
            logContext,
            assetKey
        );
        return false;
    }

    const fs::path assetRootPath = fs::path(assetRoot).lexically_normal();
    const fs::path normalizedFullPath = fullPath.lexically_normal();

    error_code errorCode{};
    const fs::path relativePath = fs::relative(normalizedFullPath, assetRootPath, errorCode).lexically_normal();
    if (errorCode || relativePath.empty() || relativePath.native().starts_with(L".."))
    {
        LOG_ERROR(
            "EffectAssetRuntimeLoader {} failed: resolved path is outside asset root. key='{}', path='{}', root='{}'",
            logContext,
            assetKey,
            To_LogPath(normalizedFullPath),
            To_LogPath(assetRootPath)
        );
        return false;
    }

    outRelativePath = relativePath.wstring();
    return true;
}

bool Resolve_EffectAssetPathByGuid(const string& effectAssetGuid, wstring& outEffectAssetPath)
{
    outEffectAssetPath.clear();

    if (effectAssetGuid.empty())
    {
        LOG_ERROR("Empty guid");
        return false;
    }

    const AssetMeta* assetMeta = GAME->Find_AssetByGUID(effectAssetGuid);
    if (nullptr == assetMeta)
    {
        LOG_WARN("Asset not found. guid='{}'", effectAssetGuid);
        return false;
    }

    if (assetMeta->type != "Effect")
    {
        LOG_ERROR("Asset type mismatch. guid='{}', type='{}'", effectAssetGuid, assetMeta->type);
        return false;
    }

    const wstring resolvedPath = GAME->Resolve_AssetPath(effectAssetGuid);
    if (resolvedPath.empty())
    {
        LOG_WARN("Resolved path is empty. guid='{}'", effectAssetGuid);
        return false;
    }

    return Try_MakeResourceRelativePath(fs::path(resolvedPath), "guid resolve", effectAssetGuid, outEffectAssetPath);
}

string Normalize_EffectNameKey(const wstring& effectName)
{
    const string effectJsonSuffix = ".effect.json";
    string key = String::ToLowerCopy(String::ToString(fs::path(effectName).filename().wstring()));
    if (key.ends_with(effectJsonSuffix))
        key.erase(key.size() - effectJsonSuffix.size());
    else
        key = fs::path(key).stem().generic_string();

    return key;
}

bool Is_EffectDefinitionAssetPath(const wstring& relativePath)
{
    const string lowerPath = String::ToLowerCopy(fs::path(relativePath).generic_string());
    return lowerPath.starts_with("effects/assets/") && lowerPath.ends_with(".effect.json");
}

bool Find_EffectAssetGuidByName(const wstring& effectName, string& outEffectAssetGuid)
{
    outEffectAssetGuid.clear();

    const string effectNameKey = Normalize_EffectNameKey(effectName);
    if (effectNameKey.empty())
    {
        LOG_ERROR("Empty effect name");
        return false;
    }

    AssetMeta matchedAsset{};
    bool hasMatch = false;
    const vector<AssetMeta> effectAssets = GAME->Get_AssetsByType("Effect");
    for (const AssetMeta& assetMeta : effectAssets)
    {
        if (!Is_EffectDefinitionAssetPath(assetMeta.relativePath))
            continue;

        const string candidateKey = Normalize_EffectNameKey(fs::path(assetMeta.relativePath).filename().wstring());
        if (candidateKey != effectNameKey)
            continue;

        if (hasMatch)
        {
            LOG_ERROR(
                "Duplicate effect asset name. name='{}', first='{}', duplicate='{}'",
                String::ToString(effectName),
                To_LogPath(matchedAsset.relativePath),
                To_LogPath(assetMeta.relativePath)
            );
            return false;
        }

        matchedAsset = assetMeta;
        hasMatch = true;
    }

    if (!hasMatch)
    {
        LOG_WARN("Effect asset name not found. name='{}'", String::ToString(effectName));
        return false;
    }

    if (matchedAsset.guid.empty())
    {
        LOG_ERROR(
            "Effect asset guid is empty. name='{}', path='{}'",
            String::ToString(effectName),
            To_LogPath(matchedAsset.relativePath)
        );
        return false;
    }

    outEffectAssetGuid = matchedAsset.guid;
    return true;
}

bool Load_JsonRoot(const wstring& effectAssetPath, json& outRoot)
{
    if (effectAssetPath.empty())
    {
        LOG_ERROR("Empty asset path");
        return false;
    }

    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
    {
        LOG_ERROR("Asset root is empty");
        return false;
    }

    const fs::path fullPath = (fs::path(assetRoot) / effectAssetPath).lexically_normal();
    if (!fs::exists(fullPath))
    {
        LOG_WARN("File not found. path='{}'", To_LogPath(fullPath));
        return false;
    }

    try
    {
        ifstream file(fullPath);
        if (!file.is_open())
        {
            LOG_ERROR("Open failed. path='{}'", To_LogPath(fullPath));
            return false;
        }

        file >> outRoot;
        return true;
    }
    catch (const exception& exception)
    {
        LOG_ERROR(
            "Json parse failed. path='{}', error='{}'",
            To_LogPath(fullPath),
            exception.what()
        );
        return false;
    }
}

}

NS_END

