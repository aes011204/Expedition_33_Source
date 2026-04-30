#include "Model.h"

#include "Mesh.h"
#include "Bone.h"
#include "Shader.h"
#include "Material.h"
#include "Animation.h"

Model::Model(ComPtr<Device> device, ComPtr<Context> context)
    : Component { device, context }
{
}

Model::Model(const Model& Prototype)
    : Component{ Prototype }
    , _aiScene { Prototype._aiScene }
    , _type{ Prototype._type }
    , _preLocalTransformMatrix{ Prototype._preLocalTransformMatrix }
     , _numMeshes { Prototype._numMeshes }
    , _meshes { Prototype._meshes }    
    , _numMaterials { Prototype._numMaterials }
    , _materials { Prototype._materials }
    , _numAnimations { Prototype._numAnimations}
    
{

    for (auto& pPrototypeAnim : Prototype._animations)
        _animations.push_back(pPrototypeAnim->Clone());

    for (auto& pPrototypeBone : Prototype._bones)
        _bones.push_back(pPrototypeBone->Clone());

    for (auto& pMaterial : _materials)
        Safe_AddRef(pMaterial);

    for (auto& pMesh : _meshes)
        Safe_AddRef(pMesh);
}

int32 Model::Get_BoneIndex(const char* pBoneName)
{
    int32    iBoneIndex = { -1 };

    auto    iter = find_if(_bones.begin(), _bones.end(), [&](Shared<Bone> pBone)->bool {
            ++iBoneIndex;
            return pBone->isCompare(pBoneName);
        });

    if (iter == _bones.end())
        return -1;

    return iBoneIndex;
}

const XMFLOAT4X4* Model::Get_BoneMatrixPtr(const char* pBoneName)
{
    auto    iter = find_if(_bones.begin(), _bones.end(), [&](Shared<Bone> pBone)->bool {    
        return pBone->isCompare(pBoneName);
        });

    if (iter == _bones.end())
        return nullptr;

    return (*iter)->Get_CombinedTransformationMatrixPtr();
}

HRESULT Model::Initialize_Prototype(ModelType eType, const char* pModelFilePath, FXMMATRIX PreLocalTransformMatrix)
{
    uint32       iFlag = { aiProcess_ConvertToLeftHanded | aiProcessPreset_TargetRealtime_Fast };

    if (ModelType::NONANIM == eType)
        iFlag |= aiProcess_PreTransformVertices;

    _aiScene = _importer.ReadFile(pModelFilePath, iFlag);
    if (nullptr == _aiScene)
        return E_FAIL;

    _type = eType;
    XMStoreFloat4x4(&_preLocalTransformMatrix, PreLocalTransformMatrix);

    
    if (FAILED(Ready_Bones(_aiScene->mRootNode, -1)))
        return E_FAIL;

    if (FAILED(Ready_Meshes()))
        return E_FAIL;

    if (FAILED(Ready_Materials(pModelFilePath)))
        return E_FAIL;

    if (FAILED(Ready_Animations()))
        return E_FAIL;



    return S_OK;
}

HRESULT Model::Initialize(void* pArg)
{
    return S_OK;
}

HRESULT Model::Render(uint32 iMeshIndex)
{
    _meshes[iMeshIndex]->Bind_Resources();
    _meshes[iMeshIndex]->Render();

    return S_OK;
}

bool Model::Play_Animation(float fTimeDelta)
{
    bool       isFinish = { false };
    isFinish = _animations[_currentAnimIndex]->Update_TransformationMatrices(fTimeDelta, _bones, _isAnimLoop);

    for (auto& pBone : _bones)
    {
        pBone->Update_CombinedTransformationMatrix(_bones, XMLoadFloat4x4(&_preLocalTransformMatrix));
    }

    return isFinish;
}

HRESULT Model::Bind_Material(Shader* pShader, const char* pConstantName, uint32 iMeshIndex, aiTextureType eMaterialType, uint32 iTextureIndex)
{
 
    return _materials[_meshes[iMeshIndex]->Get_MaterialIndex()]->Bind_Material(pShader, pConstantName, eMaterialType, iTextureIndex);

    
}

HRESULT Model::Bind_BoneMatrices(Shader* pShader, const char* pConstantName, uint32 iMeshIndex)
{

    return _meshes[iMeshIndex]->Bind_BoneMatrices(pShader, pConstantName, _bones);    
}



HRESULT Model::Ready_Meshes()
{
    _numMeshes = _aiScene->mNumMeshes;

    for (size_t i = 0; i < _numMeshes; i++)
    {
        Shared<Mesh> pMesh = Mesh::Create(_device.Get(), _context.Get(), _type, _aiScene->mMeshes[i], this, XMLoadFloat4x4(&_preLocalTransformMatrix));
        if (nullptr == pMesh)
            return E_FAIL;

        _meshes.push_back(pMesh);
    }

    return S_OK;
}

HRESULT Model::Ready_Materials(const char* pModelFilePath)
{
    _numMaterials = _aiScene->mNumMaterials;

    for (size_t i = 0; i < _numMaterials; i++)
    {
        Shared<Material> pMaterial = Material::Create(_device.Get(), _context.Get(), _aiScene->mMaterials[i], pModelFilePath);
        if (nullptr == pMaterial)
            return E_FAIL;

        _materials.push_back(pMaterial);
    }

    return S_OK;
}
HRESULT Model::Ready_Bones(const aiNode* pAINode, int32 iParentIndex)
{
    Shared<Bone> pBone = Bone::Create(pAINode, iParentIndex);
    if (nullptr == pBone)
        return E_FAIL;

    _bones.push_back(pBone);

    int32        iPIndex = _bones.size() - 1;

    for (size_t i = 0; i < pAINode->mNumChildren; i++)
    {
        Ready_Bones(pAINode->mChildren[i], iPIndex);
    }

    return S_OK;
}

HRESULT Model::Ready_Animations()
{
    _numAnimations = _aiScene->mNumAnimations;

    for (size_t i = 0; i < _numAnimations; i++)
    {
        Shared<Animation> pAnimation = Animation::Create(_aiScene->mAnimations[i], this);
        if (nullptr == pAnimation)
            return E_FAIL;

        _animations.push_back(pAnimation);        
    }  

    return S_OK;
}


Shared<Model> Model::Create(ComPtr<Device> device, ComPtr<Context> context, ModelType eType, const char* pModelFilePath, FXMMATRIX PreLocalTransformMatrix)
{
    Shared<Model> pInstance = MakeSharedFromRaw(new Model(device, context));

    if (FAILED(pInstance->Initialize_Prototype(eType, pModelFilePath, PreLocalTransformMatrix)))
    {
        MSG_BOX("Failed to Created : Model");
        Safe_Release(pInstance);
    }
    return pInstance;
}



Shared<Component> Model::Clone(void* pArg)
{
    Shared<Model> pInstance = MakeSharedFromRaw(new Model(*this));

    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : Model");
        Safe_Release(pInstance);
    }
    return pInstance;
}

void Model::Free()
{
    __super::Free();

    for (auto& pAnimation : _animations)
        Safe_Release(pAnimation);
    _animations.clear();

    for (auto& pMaterial : _materials)
        Safe_Release(pMaterial);

    _materials.clear();


    for (auto& pMesh : _meshes)
        Safe_Release(pMesh);

    _meshes.clear();

    for (auto& pBone : _bones)
        Safe_Release(pBone);
    _bones.clear();

    _importer.FreeScene();


}


