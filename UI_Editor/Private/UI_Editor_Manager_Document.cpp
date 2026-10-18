#include "pch.h"
#include "UI_Editor_Manager.h"

#include "GameInstance.h"
#include "UI_Animation_View.h"
#include "UI_DocumentInternalAccess.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
    namespace
    {
        string Normalize_PathForCompare(const string& path)
        {
            string normalizedPath = String::ToLowerCopy(path);
            replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
            return normalizedPath;
        }
    }

    bool UI_Editor_Manager::Save_Document()
    {
        if (!_fileService.Save_Document(_document, false))
            return false;

        Clear_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Save_DocumentAs()
    {
        if (!_fileService.Save_Document(_document, true))
            return false;

        Clear_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Load_Document()
    {
        if (!Confirm_SaveIfDirty(L"load another UI document"))
            return false;

        if (!_fileService.Load_Document(_document))
            return false;

        Clear_Selection();
        _animationPreviewService.Stop();
        Clear_RuntimeCaches();
        Resolve_LoadedAssetReferences();
        Refresh_AnimationTargetBindings();
        Clear_History();
        Notify_DocumentChanged(UI_EditorEditImpact::Structure);
        Clear_Dirty();
        return true;
    }

    bool UI_Editor_Manager::New_Document()
    {
        if (!Confirm_SaveIfDirty(L"create a new UI document"))
            return false;

        UI_DocumentInternalAccess::Reset(_document);
        _fileService.Clear_CurrentDocumentPath();
        Clear_Selection();
        _animationPreviewService.Stop();
        Clear_RuntimeCaches();
        Refresh_AnimationTargetBindings();
        Clear_History();
        Notify_DocumentChanged(UI_EditorEditImpact::Structure);
        Clear_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Confirm_SaveIfDirty(const wchar_t* actionName)
    {
        const UI_FileConfirmResult result = _fileService.Confirm_SaveIfDirty(Is_Dirty(), actionName, _document);
        if (result == UI_FileConfirmResult::Saved)
            Clear_Dirty();

        return result != UI_FileConfirmResult::Cancel;
    }

    void UI_Editor_Manager::Clear_RuntimeCaches()
    {
        _previewService.Clear_RuntimeCaches();
    }

    void UI_Editor_Manager::Notify_DocumentChanged(UI_EditorEditImpact impact, ObjectId objectId)
    {
        _lastDocumentChangeImpact = impact;
        _lastChangedObjectId = objectId;
        ++_documentChangeSerial;

        switch (impact)
        {
        case UI_EditorEditImpact::Selection:
            ++_selectionRevision;
            break;

        case UI_EditorEditImpact::Visual:
        case UI_EditorEditImpact::Layout:
            ++_layoutRevision;
            break;

        case UI_EditorEditImpact::Structure:
            ++_readModelRevision;
            ++_layoutRevision;
            break;

        default:
            ++_readModelRevision;
            ++_layoutRevision;
            break;
        }
    }

    void UI_Editor_Manager::Resolve_LoadedAssetReferences()
    {
        if (!_assetManagerReady)
            return;

        vector<UI_Object>& objects = UI_DocumentInternalAccess::Get_MutableObjects(_document);

        for (UI_Object& object : objects)
        {
            if (object.type == UI_ObjectType::Image)
            {
                auto resolveTextureGuid = [](string& guid, const string& path, const char* label)
                {
                    if (!guid.empty() && nullptr != GAME->Find_AssetByGUID(guid))
                        return;

                    if (path.empty())
                        return;

                    guid.clear();

                    const string targetPath = Normalize_PathForCompare(path);
                    for (const AssetMeta& assetMeta : GAME->Get_AssetsByType("Texture"))
                    {
                        if (Normalize_PathForCompare(Make_ClientRuntimeResourcePath(assetMeta)) == targetPath)
                        {
                            guid = assetMeta.guid;
                            break;
                        }
                    }

                    if (guid.empty())
                        LOG_WARN("UI Editor load: {} texture not found for {}", label, path);
                };

                resolveTextureGuid(object.textureGuid, object.texturePath, "image");
                resolveTextureGuid(object.maskTextureGuid, object.maskTexturePath, "mask");
                resolveTextureGuid(object.patternMaskTextureGuid, object.patternMaskTexturePath, "pattern mask");
                resolveTextureGuid(object.materialVfx.noiseTextureGuid, object.materialVfx.noiseTexturePath, "material vfx noise");
            }
            else if (object.type == UI_ObjectType::Text)
            {
                if (object.fontGuid.empty() && !object.fontPath.empty())
                {
                    const string targetPath = Normalize_PathForCompare(object.fontPath);
                    for (const AssetMeta& assetMeta : GAME->Get_AssetsByType("Font"))
                    {
                        if (Normalize_PathForCompare(Make_ClientRuntimeResourcePath(assetMeta)) == targetPath)
                        {
                            object.fontGuid = assetMeta.guid;
                            if (object.fontTag.empty())
                                object.fontTag = Make_FontTag(assetMeta);
                            break;
                        }
                    }

                    if (object.fontGuid.empty())
                        LOG_WARN("UI Editor load: font not found for {}", object.fontPath);
                }

                if (object.fontTag.empty() && !object.fontPath.empty())
                    object.fontTag = String::ToString(fs::path(String::ToWString(object.fontPath)).stem().wstring());
            }
        }
    }

    void UI_Editor_Manager::Refresh_AnimationTargetBindings()
    {
        const Shared<UI_Editor_Window> animationWindow = Find_Window(L"Animation");
        const Shared<UI_Animation_View> animationView = dynamic_pointer_cast<UI_Animation_View>(animationWindow);
        if (nullptr != animationView)
            animationView->Refresh_TargetBindings();
    }
}
