#include "pch.h"
#include <shellapi.h>
#include <commctrl.h>
#include "ModelConverter.h"

namespace ModelEditor
{
	::LRESULT CALLBACK ModelConverter::DragDropWndProc(::HWND hWnd, UINT message, ::WPARAM wParam, ::LPARAM lParam, ::UINT_PTR uIdSubclass, ::DWORD_PTR dwRefData)
	{
		ModelConverter* thisPointer = reinterpret_cast<ModelConverter*>(dwRefData);

		switch (message)
		{
		case WM_DROPFILES:
		{
			::HDROP hDrop = reinterpret_cast<::HDROP>(wParam);
			::uint32 fileCount = ::DragQueryFileW(hDrop, 0xffffffff, nullptr, 0);

			for (::uint32 i = 0; i < fileCount; ++i)
			{
				wchar_t filePath[MAX_PATH];
				::DragQueryFileW(hDrop, i, filePath, MAX_PATH);

				if (fs::path(filePath).extension() == ".fbx")
				{
					LOG_INFO("Drag & Drop Detected: {}", fs::path(filePath).filename().string());
					thisPointer->Convert_Model(filePath, fs::path(filePath).parent_path().wstring() + L"/");
				}
			}
			::DragFinish(hDrop);
			return 0;
		}
		case WM_NCDESTROY:
			::RemoveWindowSubclass(hWnd, DragDropWndProc, uIdSubclass);
			break;
		}
		return ::DefSubclassProc(hWnd, message, wParam, lParam);
	}

	string ModelConverter::Sanitize_FileName(string name)
	{
		string forbidden = "\\/:*?\"<>|";
		for (char& c : name)
		{
			if (forbidden.find(c) != string::npos)
			{
				c = '_';
			}
		}
		return name;
	}

	ModelConverter::ModelConverter() : EditorBlock{}
	{
	}

	ModelConverter::~ModelConverter()
	{
		if (_hWnd)
		{
			::RemoveWindowSubclass(_hWnd, DragDropWndProc, 0);
		}
        _hWnd = nullptr;
        ZeroMemory(&_pathBuffer, sizeof(char) * 512);
        Clear_Converter();
	}

	HRESULT ModelConverter::Initialize(::HWND hWnd)
	{
		Clear_Converter();
		importFlags = aiProcess_ConvertToLeftHanded |
					  aiProcess_Triangulate |
					  aiProcess_CalcTangentSpace |
					  aiProcess_GenSmoothNormals |
					  aiProcess_PopulateArmatureData;

		strcpy_s(_pathBuffer, Engine::String::ToString(ResourcePath).c_str());
		_resourcePath = ResourcePath;

		_hWnd = hWnd;

		if (_hWnd)
		{
			::DragAcceptFiles(_hWnd, TRUE);
			::SetWindowSubclass(_hWnd, DragDropWndProc, 0, reinterpret_cast<::DWORD_PTR>(this));
		}

		return S_OK;
	}

	void ModelConverter::Update(float timeDelta)
	{
	}

	void ModelConverter::Render()
	{
		ImGui::Begin("Model Converter");

		ImGui::Text("Resource Path:");
		if (ImGui::InputText("##ResPath", _pathBuffer, sizeof(_pathBuffer)))
		{
			_resourcePath = String::ToWString(_pathBuffer);
		}
		ImGui::Separator();

		if (ImGui::Button("Extract All (.fbx)", ImVec2(-1, 30)))
		{
			ExtractAll(_resourcePath);
		}
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

		if (ImGui::BeginChild("DropInfo", ImVec2(0, 30), true, ::ImGuiWindowFlags_NoMove))
		{
			ImGui::Text("Can Drag & Drop FBX File to Window");
		}

		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::End();
	}

	void ModelConverter::ExtractAll(const wstring& path)
	{
		if (!fs::exists(path) || !fs::is_directory(path))
		{
			LOG_ERROR("Invalid directory path: {}", String::ToString(path));
			return;
		}

		LOG_INFO(">>> Start Bulk Extraction: {}", String::ToString(path));
		::uint32 successCount = 0;
		::uint32 failCount = 0;

		for (const auto& entry : fs::recursive_directory_iterator(path))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".fbx")
			{
				wstring fbxPath = entry.path().wstring();
				wstring outputDir = entry.path().parent_path().wstring() + L"/";
				LOG_INFO("Target found: {}", entry.path().filename().string());
				if (SUCCEEDED(Convert_Model(fbxPath, outputDir)))
				{
					successCount++;
				}
				else
				{
					LOG_ERROR("Failed to convert: {}", entry.path().filename().string());
					failCount++;
				}
			}
		}
		LOG_INFO(">>> Extraction Finished. (Success: {}, Fail: {})", successCount, failCount);
	}

	HRESULT ModelConverter::Convert_Model(const wstring& fbxFilePath, const wstring& outputDir)
	{
		Clear_Converter();
		string utf8FilePath = Engine::String::ToString(fbxFilePath);
		_aiScene = _importer.ReadFile(utf8FilePath, importFlags);

		if (nullptr == _aiScene || _aiScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !_aiScene->mRootNode)
		{
			LOG_ERROR("Assimp Error: {} \n", _importer.GetErrorString());
			return E_FAIL;
		}

		string modelName = Get_FileNameWithoutExtension(fbxFilePath);
		_isSkeletal = _aiScene->HasSkeletons() || _aiScene->mNumAnimations > 0;

		ReadNodes(_aiScene->mRootNode, -1);
		ReadMeshes(_aiScene);
		ReadMaterial(_aiScene);
		ReadAnimations(_aiScene);

		Ensure_DirectoryExists(outputDir);
		if (FAILED(Export_ModelData(_aiScene, outputDir, modelName)))
		{
			return E_FAIL;
		}
		if (FAILED(Export_AnimationData(_aiScene, outputDir, modelName)))
		{
			return E_FAIL;
		}

		LOG_INFO("Convert Success: {}", modelName);

		return S_OK;
	}

	void ModelConverter::ReadNodes(const aiNode* node, int32 parentIndex)
	{
		ModelNodeData nodeData{};

		strncpy_s(nodeData.name, node->mName.C_Str(), maxModelNameSize - 1);
		nodeData.prentIndex = parentIndex;
		nodeData.localTransform = Convert_Matrix(node->mTransformation);

		_nodes.push_back(nodeData);
		int32 curIndex = static_cast<int32>(_nodes.size() - 1);

		for (uint32 i = 0; i < node->mNumChildren; ++i)
		{
			ReadNodes(node->mChildren[i], curIndex);
		}
	}

	void ModelConverter::ReadMeshes(const aiScene* aiScene)
	{
		for (uint32 i = 0; i < aiScene->mNumMeshes; ++i)
		{
			ModelMeshData meshData{};
			const aiMesh* aiMesh = aiScene->mMeshes[i];

			strncpy_s(meshData.name, aiMesh->mName.C_Str(), maxModelNameSize - 1);
			meshData.materialIndex = aiMesh->mMaterialIndex;
			meshData.numVertices = aiMesh->mNumVertices;
			meshData.numIndices = aiMesh->mNumFaces * 3;

			vector<uint32> index;
			for (uint32 j = 0; j < aiMesh->mNumFaces; ++j)
			{
				for (uint32 k = 0; k < 3; ++k)
				{
					index.push_back(aiMesh->mFaces[j].mIndices[k]);
				}
			}

			if (_isSkeletal)
			{
				vector<uint32> boneRefs;
				vector<Matrix> offsets;
				vector<VTXANIMMESH> vertices(aiMesh->mNumVertices, VTXANIMMESH{});

				for (uint32 j = 0; j < aiMesh->mNumVertices; ++j)
				{
					vertices[j].position = Vec3{ aiMesh->mVertices[j].x, aiMesh->mVertices[j].y, aiMesh->mVertices[j].z };
					if (aiMesh->HasNormals())
					{
						vertices[j].normal = Vec3{ aiMesh->mNormals[j].x, aiMesh->mNormals[j].y, aiMesh->mNormals[j].z };
					}
					if (aiMesh->HasTangentsAndBitangents())
					{
						vertices[j].tangent = Vec3{ aiMesh->mTangents[j].x, aiMesh->mTangents[j].y, aiMesh->mTangents[j].z };
						vertices[j].binormal = Vec3{ aiMesh->mBitangents[j].x, aiMesh->mBitangents[j].y, aiMesh->mBitangents[j].z };
					}
					if (aiMesh->HasTextureCoords(0))
					{
						vertices[j].texcoord = Vec2{ aiMesh->mTextureCoords[0][j].x, aiMesh->mTextureCoords[0][j].y };
					}
				}

				for (uint32 j = 0; j < aiMesh->mNumBones; ++j)
				{
					const aiBone* aiBone = aiMesh->mBones[j];
					string boneName = aiBone->mName.C_Str();

					if (false == _boneNameMap.contains(boneName))
					{
						ModelBoneData boneData{};
						strncpy_s(boneData.name, boneName.c_str(), maxModelNameSize - 1);
						boneData.offsetMatrix = Convert_Matrix(aiBone->mOffsetMatrix);
						boneData.nodeIndex = FindNodeIndex(boneName.c_str());
						_boneNameMap[boneName] = static_cast<uint32>(_bones.size());
						_bones.push_back(boneData);
					}

					boneRefs.push_back(_boneNameMap[boneName]);
					offsets.push_back(Convert_Matrix(aiBone->mOffsetMatrix).Transpose());

					for (uint32 k = 0; k < aiBone->mNumWeights; ++k)
					{
						uint32 vertexIndex = aiBone->mWeights[k].mVertexId;
						float weight = aiBone->mWeights[k].mWeight;

						auto& vertex = vertices[vertexIndex];
						if (vertex.blendWeight.x == 0.f)
						{
							vertex.blendIndex.x = j;
							vertex.blendWeight.x = weight;
						}
						else if (vertex.blendWeight.y == 0.f)
						{
							vertex.blendIndex.y = j;
							vertex.blendWeight.y = weight;
						}
						else if (vertex.blendWeight.z == 0.f)
						{
							vertex.blendIndex.z = j;
							vertex.blendWeight.z = weight;
						}
						else if (vertex.blendWeight.w == 0.f)
						{
							vertex.blendIndex.w = j;
							vertex.blendWeight.w = weight;
						}
					}
				}

				meshData.numBoneRefs = static_cast<uint32>(boneRefs.size());
				_animVertices.push_back(move(vertices));
				_meshBoneRefs.push_back(move(boneRefs));
				_meshOffsets.push_back(move(offsets));
				_staticVertices.push_back({});
			}
			else
			{
				vector<VTXMESH> vertices(aiMesh->mNumVertices, VTXMESH{});

				for (uint32 j = 0; j < aiMesh->mNumVertices; ++j)
				{
					vertices[j].position = Vec3{ aiMesh->mVertices[j].x, aiMesh->mVertices[j].y, aiMesh->mVertices[j].z };
					if (aiMesh->HasNormals())
					{
						vertices[j].normal = Vec3{ aiMesh->mNormals[j].x, aiMesh->mNormals[j].y, aiMesh->mNormals[j].z };
					}
					if (aiMesh->HasTangentsAndBitangents())
					{
						vertices[j].tangent = Vec3{ aiMesh->mTangents[j].x, aiMesh->mTangents[j].y, aiMesh->mTangents[j].z };
						vertices[j].binormal = Vec3{ aiMesh->mBitangents[j].x, aiMesh->mBitangents[j].y, aiMesh->mBitangents[j].z };
					}
					if (aiMesh->HasTextureCoords(0))
					{
						vertices[j].texcoord = Vec2{ aiMesh->mTextureCoords[0][j].x, aiMesh->mTextureCoords[0][j].y };
					}
				}

				meshData.numBoneRefs = 0;
				_staticVertices.push_back(move(vertices));
				_animVertices.push_back({});
				_meshBoneRefs.push_back({});
				_meshOffsets.push_back({});
			}

			_meshHeaders.push_back(meshData);
			_indices.push_back(move(index));
		}
	}

	void ModelConverter::ReadMaterial(const aiScene* aiScene)
	{
		for (uint32 i = 0; i < aiScene->mNumMaterials; ++i)
		{
			const aiMaterial* aiMaterial = aiScene->mMaterials[i];
			ModelMaterialData materialData{};
			aiString materialName;
			aiMaterial->Get(AI_MATKEY_NAME, materialName);
			strncpy_s(materialData.name, materialName.C_Str(), maxModelNameSize - 1);

			aiColor3D aiColor{};
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor))
				materialData.diffuseColor = Vec4{ aiColor.r, aiColor.g, aiColor.b, 1.f };
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiColor))
				materialData.emissiveColor = Vec4{ aiColor.r, aiColor.g, aiColor.b, 1.f };

			float scalar{ 0.f };
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, scalar))
				materialData.roughness = scalar;
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, scalar))
				materialData.metalness = scalar;
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_SHININESS, scalar))
				materialData.shininess = scalar;
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_OPACITY, scalar))
				materialData.opacity = scalar;

			vector<ModelTextureSlot> slots;
			for (uint32 j = 0; j < AI_TEXTURE_TYPE_MAX; ++j)
			{
				uint32 num = aiMaterial->GetTextureCount(static_cast<aiTextureType>(j));
				for (uint32 k = 0; k < num; ++k)
				{
					aiString texPath;
					uint32 uvIndex = 0;
					ai_real blend = 1.f;
					aiTextureOp op = aiTextureOp_Multiply;
					aiTextureMapMode wm = aiTextureMapMode_Wrap;

					aiMaterial->GetTexture(static_cast<aiTextureType>(j), k, &texPath, nullptr, &uvIndex, &blend, &op, &wm);

					aiUVTransform uvTrans{};
					aiMaterial->Get(AI_MATKEY_UVTRANSFORM(static_cast<aiTextureType>(j), k), uvTrans);

					string filePath = texPath.C_Str();
					size_t lastPos = filePath.find_last_of("\\/");
					string fileName = (lastPos != string::npos) ? filePath.substr(lastPos + 1) : filePath;

					ModelTextureSlot slot{};
					slot.typeIndex = j;
					slot.textureIndex = k;
					strncpy_s(slot.path, fileName.c_str(), maxModelNameSize - 1);

					slot.uvScale = Vec2{ uvTrans.mScaling.x != 0.f ? uvTrans.mScaling.x : 1.f, uvTrans.mScaling.y != 0.f ? uvTrans.mScaling.y : 1.f };
					slot.uvOffset = Vec2{ uvTrans.mTranslation.x, uvTrans.mTranslation.y };
					slot.uvRotation = uvTrans.mRotation;
					slot.blendFactor = blend;
					slot.blendOp = static_cast<uint32>(op);
					slot.uvChannel = uvIndex;
					slot.wrapMode = static_cast<uint32>(wm);
					slots.push_back(slot);
				}
			}

			materialData.numSlots = static_cast<uint32>(slots.size());
			_matHeaders.push_back(materialData);
			_matSlots.push_back(move(slots));
		}
	}

	void ModelConverter::ReadAnimations(const aiScene* aiScene)
	{
		if (!_isSkeletal) return;

		for (uint32 i = 0; i < aiScene->mNumAnimations; ++i)
		{
			const aiAnimation* aiAnim = aiScene->mAnimations[i];
			ModelAnimationData animData{};
			strncpy_s(animData.name, aiAnim->mName.C_Str(), maxModelNameSize - 1);
			animData.duration = aiAnim->mDuration;
			animData.ticksPerSecond = aiAnim->mTicksPerSecond;
			animData.numChannels = aiAnim->mNumChannels;

			aiNodeAnim* rootChannel{ nullptr };
			int32 minParentIndex = INT32_MAX;

			for (uint32 j = 0; j < aiAnim->mNumChannels; ++j)
			{
				int32 nodeIndex = FindNodeIndex(aiAnim->mChannels[j]->mNodeName.C_Str());
				if (nodeIndex >= 0 && _nodes[nodeIndex].prentIndex < minParentIndex)
				{
					minParentIndex = _nodes[nodeIndex].prentIndex;
					rootChannel = aiAnim->mChannels[j];
					if (minParentIndex == -1)
						break;
				}
			}

			if (rootChannel)
			{
				aiVector3D startPosition = rootChannel->mPositionKeys[0].mValue;
				aiVector3D endPosition = rootChannel->mPositionKeys[rootChannel->mNumPositionKeys - 1].mValue;
				animData.rootTotalTranslation = Vec3{ endPosition.x - startPosition.x, endPosition.y - startPosition.y, endPosition.z - startPosition.z };

				aiQuaternion startRotation = rootChannel->mRotationKeys[0].mValue;
				aiQuaternion endRotation = rootChannel->mRotationKeys[rootChannel->mNumRotationKeys - 1].mValue;

				aiQuaternion inverseStartRotation = startRotation;
				inverseStartRotation.Conjugate();

				aiQuaternion deltaRotation = endRotation * inverseStartRotation;
				animData.rootTotalRotation = Vec4{ deltaRotation.x, deltaRotation.y, deltaRotation.z, deltaRotation.w };
			}

			vector<ModelChannelData> channels;
			vector<vector<KEYFRAME>> keyFrames;
			for (uint32 j = 0; j < aiAnim->mNumChannels; ++j)
			{
				const aiNodeAnim* animNode = aiAnim->mChannels[j];
				ModelChannelData channelData{};
				strncpy_s(channelData.boneName, animNode->mNodeName.C_Str(), maxModelNameSize - 1);
				channelData.boneIndex = FindNodeIndex(channelData.boneName);

				uint32 numKeys = max({ animNode->mNumPositionKeys, animNode->mNumRotationKeys, animNode->mNumScalingKeys });
				channelData.numKeyframes = numKeys;

				vector<KEYFRAME> frames(numKeys);
				for (uint32 k = 0; k < numKeys; ++k)
				{
					uint32 posIdx = min(k, animNode->mNumPositionKeys - 1);
					uint32 rotIdx = min(k, animNode->mNumRotationKeys - 1);
					uint32 scaleIdx = min(k, animNode->mNumScalingKeys - 1);

					frames[k].trackPosition = static_cast<float>(animNode->mPositionKeys[posIdx].mTime);

					aiVector3D pos = animNode->mPositionKeys[posIdx].mValue;
					frames[k].translation = Vec3{ pos.x, pos.y, pos.z };

					aiQuaternion rot = animNode->mRotationKeys[rotIdx].mValue;
					frames[k].rotation = Quat{ rot.x, rot.y, rot.z, rot.w };

					aiVector3D scale = animNode->mScalingKeys[scaleIdx].mValue;
					frames[k].scale = Vec3{ scale.x, scale.y, scale.z };
				}

				channels.push_back(channelData);
				keyFrames.push_back(move(frames));
			}

			_animHeaders.push_back(animData);
			_channelHeaders.push_back(move(channels));
			_keyframes.push_back(move(keyFrames));
		}
	}

	::HRESULT ModelConverter::Export_ModelData(const aiScene* aiScene, const wstring& outputDir, const string& modelName)
	{
		wstring meshPath = outputDir + Engine::String::ToWString(modelName) + L".model";
		ofstream out(meshPath, ios::binary);
		if (!out.is_open()) return E_FAIL;

		::uint32 numNodes = static_cast<::uint32>(_nodes.size());
		::uint32 numMaterials = static_cast<::uint32>(_matHeaders.size());
		::uint32 numMeshes = static_cast<::uint32>(_meshHeaders.size());
		::uint32 numBones = static_cast<::uint32>(_bones.size());

		out.write(BYTE(&_isSkeletal), sizeof(bool));
		out.write(BYTE(&numNodes), sizeof(::uint32));
		out.write(BYTE(&numMaterials), sizeof(::uint32));
		out.write(BYTE(&numMeshes), sizeof(::uint32));
		out.write(BYTE(&numBones), sizeof(::uint32));

		out.write(BYTE(_nodes.data()), numNodes * sizeof(ModelNodeData));

		for (::uint32 i = 0; i < numMaterials; ++i)
		{
			out.write(BYTE(&_matHeaders[i]), sizeof(ModelMaterialData));
			out.write(BYTE(_matSlots[i].data()), _matHeaders[i].numSlots * sizeof(ModelTextureSlot));
		}

		for (::uint32 i = 0; i < numMeshes; ++i)
		{
			out.write(BYTE(&_meshHeaders[i]), sizeof(ModelMeshData));
			if (_isSkeletal)
			{
				out.write(BYTE(_meshBoneRefs[i].data()), _meshHeaders[i].numBoneRefs * sizeof(::uint32));
				out.write(BYTE(_meshOffsets[i].data()), _meshHeaders[i].numBoneRefs * sizeof(Matrix));
				out.write(BYTE(_animVertices[i].data()), _meshHeaders[i].numVertices * sizeof(VTXANIMMESH));
			}
			else
			{
				out.write(BYTE(_staticVertices[i].data()), _meshHeaders[i].numVertices * sizeof(VTXMESH));
			}
			out.write(BYTE(_indices[i].data()), _meshHeaders[i].numIndices * sizeof(::uint32));
		}

		if (numBones > 0)
		{
			out.write(BYTE(_bones.data()), numBones * sizeof(ModelBoneData));
		}

		out.close();
		return S_OK;
	}

	::HRESULT ModelConverter::Export_AnimationData(const aiScene* aiScene, const wstring& outputDir, const string& modelName)
	{
		if (!_isSkeletal || _animHeaders.empty()) return S_OK;

		for (::uint32 i = 0; i < _animHeaders.size(); ++i)
		{
			string animClipName = Sanitize_FileName(string(_animHeaders[i].name));
			string fullFileName = modelName + "_" + animClipName;

			wstring animPath = outputDir + Engine::String::ToWString(fullFileName) + L".anim";
			ofstream out(animPath, ios::binary);
			if (!out.is_open())
			{
				LOG_ERROR("Failed to create anim file: {}", fullFileName);
				continue;
			}

			out.write(BYTE(&_animHeaders[i]), sizeof(ModelAnimationData));

			::uint32 numChannels = _animHeaders[i].numChannels;
			for (::uint32 j = 0; j < numChannels; ++j)
			{
				out.write(BYTE(&_channelHeaders[i][j]), sizeof(ModelChannelData));
				out.write(BYTE(_keyframes[i][j].data()), _channelHeaders[i][j].numKeyframes * sizeof(KEYFRAME));
			}

			out.close();
			LOG_INFO("Export Anim Clip: {}", fullFileName);
		}

		return S_OK;
	}

	Matrix ModelConverter::Convert_Matrix(const aiMatrix4x4& aiMat) const
	{
		Matrix mat;
		mat._11 = aiMat.a1; mat._12 = aiMat.b1; mat._13 = aiMat.c1; mat._14 = aiMat.d1;
		mat._21 = aiMat.a2; mat._22 = aiMat.b2; mat._23 = aiMat.c2; mat._24 = aiMat.d2;
		mat._31 = aiMat.a3; mat._32 = aiMat.b3; mat._33 = aiMat.c3; mat._34 = aiMat.d3;
		mat._41 = aiMat.a4; mat._42 = aiMat.b4; mat._43 = aiMat.c4; mat._44 = aiMat.d4;

		return mat;
	}

	int32 ModelConverter::FindNodeIndex(const char* name)
	{
		for (int32 i = 0; i < static_cast<int32>(_nodes.size()); ++i)
		{
			if (!strcmp(_nodes[i].name, name))
			{
				return i;
			}
		}
		return -1;
	}

	void ModelConverter::Clear_Converter()
	{
		_aiScene = nullptr;

		_nodes.clear();
		_meshHeaders.clear();
		_staticVertices.clear();
		_animVertices.clear();
		_indices.clear();
		_meshBoneRefs.clear();
		_meshOffsets.clear();
		_matHeaders.clear();
		_matSlots.clear();
		_bones.clear();
		_boneNameMap.clear();
		_animHeaders.clear();
		_channelHeaders.clear();
		_keyframes.clear();
	}

	string ModelConverter::Get_FileNameWithoutExtension(const wstring& filePath) const
	{
		fs::path path(filePath);
		return path.stem().string();
	}

	void ModelConverter::Ensure_DirectoryExists(const wstring& dirPath) const
	{
		if (!fs::exists(dirPath))
		{
			fs::create_directories(dirPath);
		}
	}

	Shared<ModelConverter> ModelConverter::Create(::HWND hWnd)
	{
		auto prototype = make_shared<ModelConverter>();

		if (FAILED(prototype->Initialize(hWnd)))
		{
			MSG_BOX("Failed to Created : ModelConverter");
			return nullptr;
		}

		return prototype;
	}

}
