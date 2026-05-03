#include "ModelCom.h"

#include "Mesh.h"
#include "Bone.h"
#include "ShaderCom.h"
#include "Material.h"
#include "Animation.h"

ModelCom::ModelCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component { device, context }
{
}

ModelCom::ModelCom(const ModelCom& prototype)
    : Component{ prototype }
    , _aiScene { prototype._aiScene }
    , _type{ prototype._type }
    , _preLocalTransformMatrix{ prototype._preLocalTransformMatrix }
     , _numMeshes { prototype._numMeshes }
    , _meshes { prototype._meshes }    
    , _numMaterials { prototype._numMaterials }
    , _materials { prototype._materials }
    , _numAnimations { prototype._numAnimations}
    
{

    for (auto& prototypeAnim : prototype._animations)
        _animations.push_back(prototypeAnim->Clone());

    for (auto& prototypeBone : prototype._bones)
        _bones.push_back(prototypeBone->Clone());
}

int32 ModelCom::Get_BoneIndex(const char* boneName)
{
    int32    boneIndex = { -1 };

    auto    iter = find_if(_bones.begin(), _bones.end(), [&](Shared<Bone> bone)->bool {
            ++boneIndex;
            return bone->isCompare(boneName);
        });

    if (iter == _bones.end())
        return -1;

    return boneIndex;
}

const XMFLOAT4X4* ModelCom::Get_BoneMatrixPtr(const char* boneName)
{
    auto    iter = find_if(_bones.begin(), _bones.end(), [&](Shared<Bone> bone)->bool {    
        return bone->isCompare(boneName);
        });

    if (iter == _bones.end())
        return nullptr;

    return (*iter)->Get_CombinedTransformationMatrixPtr();
}

HRESULT ModelCom::Initialize_Prototype(ModelType type, const char* modelFilePath, FXMMATRIX preLocalTransformMatrix)
{
    uint32       flag = { aiProcess_ConvertToLeftHanded | aiProcessPreset_TargetRealtime_Fast };

    if (ModelType::NonAnim == type)
        flag |= aiProcess_PreTransformVertices;

    _aiScene = _importer.ReadFile(modelFilePath, flag);
    if (nullptr == _aiScene)
        return E_FAIL;

    _type = type;
    XMStoreFloat4x4(&_preLocalTransformMatrix, preLocalTransformMatrix);

    
    if (FAILED(Ready_Bones(_aiScene->mRootNode, -1)))
        return E_FAIL;

    if (FAILED(Ready_Meshes()))
        return E_FAIL;

    if (FAILED(Ready_Materials(modelFilePath)))
        return E_FAIL;

    if (FAILED(Ready_Animations()))
        return E_FAIL;



    return S_OK;
}

HRESULT ModelCom::Initialize(void* arg)
{
    return S_OK;
}

HRESULT ModelCom::Render(uint32 meshIndex)
{
    _meshes[meshIndex]->Bind_Resources();
    _meshes[meshIndex]->Render();

    return S_OK;
}

bool ModelCom::Play_Animation(float timeDelta)
{
    bool       isFinish = { false };
    isFinish = _animations[_currentAnimIndex]->Update_TransformationMatrices(timeDelta, _bones, _isAnimLoop);

    for (auto& bone : _bones)
    {
        bone->Update_CombinedTransformationMatrix(_bones, XMLoadFloat4x4(&_preLocalTransformMatrix));
    }

    return isFinish;
}

HRESULT ModelCom::Bind_Material(ShaderCom* shader, const char* constantName, uint32 meshIndex, aiTextureType materialType, uint32 textureIndex)
{
 
    return _materials[_meshes[meshIndex]->Get_MaterialIndex()]->Bind_Material(shader, constantName, materialType, textureIndex);

    
}

HRESULT ModelCom::Bind_BoneMatrices(ShaderCom* shader, const char* constantName, uint32 meshIndex)
{

    return _meshes[meshIndex]->Bind_BoneMatrices(shader, constantName, _bones);    
}



HRESULT ModelCom::Ready_Meshes()
{
    _numMeshes = _aiScene->mNumMeshes;

    for (size_t i = 0; i < _numMeshes; i++)
    {
        Shared<Mesh> mesh = Mesh::Create(_device.Get(), _context.Get(), _type, _aiScene->mMeshes[i], this, XMLoadFloat4x4(&_preLocalTransformMatrix));
        if (nullptr == mesh)
            return E_FAIL;

        _meshes.push_back(mesh);
    }

    return S_OK;
}

HRESULT ModelCom::Ready_Materials(const char* modelFilePath)
{
    _numMaterials = _aiScene->mNumMaterials;

    for (size_t i = 0; i < _numMaterials; i++)
    {
        Shared<Material> material = Material::Create(_device.Get(), _context.Get(), _aiScene->mMaterials[i], modelFilePath);
        if (nullptr == material)
            return E_FAIL;

        _materials.push_back(material);
    }

    return S_OK;
}
HRESULT ModelCom::Ready_Bones(const aiNode* aiNode, int32 parentIndex)
{
    Shared<Bone> bone = Bone::Create(aiNode, parentIndex);
    if (nullptr == bone)
        return E_FAIL;

    _bones.push_back(bone);

    int32        index = static_cast<int32>(_bones.size() - 1);

    for (size_t i = 0; i < aiNode->mNumChildren; i++)
    {
        Ready_Bones(aiNode->mChildren[i], index);
    }

    return S_OK;
}

HRESULT ModelCom::Ready_Animations()
{
    _numAnimations = _aiScene->mNumAnimations;

    for (size_t i = 0; i < _numAnimations; i++)
    {
        Shared<Animation> animation = Animation::Create(_aiScene->mAnimations[i], this);
        if (nullptr == animation)
            return E_FAIL;

        _animations.push_back(animation);        
    }  

    return S_OK;
}


Shared<ModelCom> ModelCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, ModelType type, const char* modelFilePath, FXMMATRIX preLocalTransformMatrix)
{
    auto instance = make_shared<ModelCom>(device, context);

    if (FAILED(instance->Initialize_Prototype(type, modelFilePath, preLocalTransformMatrix)))
    {
        MSG_BOX("Failed to Created : ModelCom");
        return nullptr;
	}
    return instance;
}



Shared<Component> ModelCom::Clone(void* arg)
{
    auto instance = make_shared<ModelCom>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : ModelCom");
        return nullptr;
	}
    return instance;
}

void ModelCom::Free()
{
    __super::Free();

    _animations.clear();
    _materials.clear();
    _meshes.clear();


    _bones.clear();

    _importer.FreeScene();


}


