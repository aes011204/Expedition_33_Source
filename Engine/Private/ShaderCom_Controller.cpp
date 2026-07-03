#include "ShaderCom_Controller.h"

#include "ShaderCom.h"
#include <algorithm>

NS_BEGIN(Engine)

ShaderCom_Controller::ShaderCom_Controller(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT ShaderCom_Controller::Initialize()
{
    if (_device == nullptr || _context == nullptr)
        return E_FAIL;

    return S_OK;
}

bool ShaderCom_Controller::Is_ReloadTarget(ShaderType shaderType)
{
    // VtxMesh includes custom mesh shaders such as Shader_VictoriaFlag.
    return shaderType == ShaderType::VtxMesh
        || shaderType == ShaderType::VtxAnimMesh
        || shaderType == ShaderType::VtxInstanceMeshExact;
}

const ShaderCom_Controller::ShaderRecord* ShaderCom_Controller::Find_Record_Locked(const wstring& shaderPath) const
{
    auto iter = std::find_if(_records.begin(), _records.end(),
        [&shaderPath](const ShaderRecord& record)
        {
            return record.shaderPath == shaderPath;
        });

    return iter == _records.end() ? nullptr : &(*iter);
}

ShaderCom_Controller::ShaderRecord* ShaderCom_Controller::Find_Record_Locked(const wstring& shaderPath)
{
    auto iter = std::find_if(_records.begin(), _records.end(),
        [&shaderPath](const ShaderRecord& record)
        {
            return record.shaderPath == shaderPath;
        });

    return iter == _records.end() ? nullptr : &(*iter);
}

void ShaderCom_Controller::Prune_ExpiredShaders_Locked(ShaderRecord& record)
{
    vector<Weak<ShaderCom>> liveShaders;
    liveShaders.reserve(record.shaders.size());

    for (auto& weakShader : record.shaders)
    {
        if (auto shader = weakShader.lock())
            liveShaders.emplace_back(shader);
    }

    record.shaders = std::move(liveShaders);
}

HRESULT ShaderCom_Controller::Register_Shader(const Shared<ShaderCom>& shader)
{
    if (shader == nullptr)
        return E_INVALIDARG;

    if (!Is_ReloadTarget(shader->Get_Type()))
        return S_OK;

    lock_guard<mutex> lock{ _mutex };

    ShaderRecord* record = Find_Record_Locked(shader->Get_ShaderFilePath());
    if (record == nullptr)
    {
        _records.emplace_back();
        record = &_records.back();
        record->shaderPath = shader->Get_ShaderFilePath();
        record->shaderType = shader->Get_Type();
        record->inputElements = shader->Get_InputElements();
        record->numElements = shader->Get_InputElementCount();
    }

    const auto alreadyRegistered = std::find_if(record->shaders.begin(), record->shaders.end(),
        [&shader](const Weak<ShaderCom>& weakShader)
        {
            auto locked = weakShader.lock();
            return locked != nullptr && locked.get() == shader.get();
        });

    if (alreadyRegistered == record->shaders.end())
        record->shaders.emplace_back(shader);

    Prune_ExpiredShaders_Locked(*record);
    return S_OK;
}

void ShaderCom_Controller::Unregister_Shader(const ShaderCom* shader)
{
    if (shader == nullptr)
        return;

    lock_guard<mutex> lock{ _mutex };

    for (auto recordIter = _records.begin(); recordIter != _records.end(); )
    {
        ShaderRecord& record = *recordIter;

        record.shaders.erase(
            remove_if(record.shaders.begin(), record.shaders.end(),
                [shader](const Weak<ShaderCom>& weakShader)
                {
                    auto locked = weakShader.lock();
                    return locked == nullptr || locked.get() == shader;
                }),
            record.shaders.end());

        if (record.shaders.empty())
        {
            recordIter = _records.erase(recordIter);
            continue;
        }

        ++recordIter;
    }
}

HRESULT ShaderCom_Controller::Reload_Record(ShaderRecord& record)
{
    if (!Is_ReloadTarget(record.shaderType))
        return S_OK;

    if (record.shaderPath.empty() || record.inputElements.empty() || record.numElements == 0)
        return E_FAIL;

    Shared<ShaderCom> recompiled = ShaderCom::Create(
        _device,
        _context,
        record.shaderPath.c_str(),
        record.inputElements.data(),
        record.numElements,
        record.shaderType,
        false);

    if (recompiled == nullptr)
        return E_FAIL;

    vector<Shared<ShaderCom>> liveShaders;
    liveShaders.reserve(record.shaders.size());

    for (auto& weakShader : record.shaders)
    {
        if (auto shader = weakShader.lock())
            liveShaders.emplace_back(shader);
    }

    for (const Shared<ShaderCom>& shader : liveShaders)
        CHECK_FAILED(shader->Recreate(*recompiled), E_FAIL);

    record.shaders.clear();
    for (const Shared<ShaderCom>& shader : liveShaders)
        record.shaders.emplace_back(shader);

    return S_OK;
}

HRESULT ShaderCom_Controller::Reload_VertexShaders()
{
    lock_guard<mutex> lock{ _mutex };

    uint32 successCount = 0;
    uint32 targetCount = 0;

    for (ShaderRecord& record : _records)
    {
        if (!Is_ReloadTarget(record.shaderType))
            continue;

        Prune_ExpiredShaders_Locked(record);
        if (record.shaders.empty())
            continue;

        ++targetCount;
        if (SUCCEEDED(Reload_Record(record)))
            ++successCount;
    }

    if (targetCount == 0)
        return E_FAIL;

    if (successCount == targetCount)
        return S_OK;

    if (successCount > 0)
        return S_FALSE;

    return E_FAIL;
}

Unique<ShaderCom_Controller> ShaderCom_Controller::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_unique<ShaderCom_Controller>(device, context);

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : ShaderCom_Controller");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void ShaderCom_Controller::Free()
{
    __super::Free();

    lock_guard<mutex> lock{ _mutex };
    _records.clear();
}

NS_END
