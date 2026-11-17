#include "GeometryImport.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "ChartBuilder.h"
#include "Mesh.h"
#include "DirectXMath.h"
#include "RHI/RHIStructures.h"
#include "../Animation/AnimatorComponent.h"
#include "../Animation/Clip.h"
#include "../Resources/ResourceManager.h"
#include "../Engine/Scene/SceneNode.h"

using namespace DirectX;

// Assimp matrices act on column vectors; every one is transposed into DirectXMath row-vector form.
static XMMATRIX ToMatrix(const aiMatrix4x4& matrix)
{
    return XMMatrixTranspose(XMMATRIX(&matrix.a1));
}

static bool IsNearlyEqual(const XMFLOAT4X4& a, const XMFLOAT4X4& b)
{
    for (int row = 0; row < 4; row++)
        for (int column = 0; column < 4; column++)
            if (std::abs(a.m[row][column] - b.m[row][column]) > 1e-3f * (std::max)(1.0f, std::abs(a.m[row][column])))
                return false;
    return true;
}

// What one import knows: the model root, and for a skinned model its skeleton, the joint of each
// joint node and every node's bind-pose transform into model space (the model root's frame).
struct ImportState
{
    const aiScene* Scene = nullptr;
    std::string Name;
    std::string DestDir;
    SceneNode* Root = nullptr;
    Skeleton* ModelSkeleton = nullptr;
    float MillimetresPerUnit = 10.0f;
    double ChartMilliseconds = 0.0;
    std::unordered_map<const aiNode*, int32_t> JointByNode;
    std::unordered_map<std::string, int32_t> JointByName;
    std::unordered_map<const aiNode*, XMFLOAT4X4> ModelByNode;

    XMMATRIX GetModel(const aiNode* node) const { return XMLoadFloat4x4(&ModelByNode.at(node)); }
    bool IsJoint(const aiNode* node) const { return JointByNode.contains(node); }
};

// Chart sizes for every mesh with UVs, and chart maps for skinned ones, in millimetres of the bind
// pose as the file measures it; the scene may rescale the model, which charts ignore.
static void MeasureChart(Mesh& mesh, ImportState& state)
{
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::vector<ChartBuilder::ChartVertex> vertices(mesh.GetVertexCount());
    const uint32_t* indices = static_cast<const uint32_t*>(mesh.GetCachedIndexData());
    if (mesh.IsSkinned())
    {
        const RHIStructures::SkinnedVertex* source = static_cast<const RHIStructures::SkinnedVertex*>(mesh.GetCachedVertexData());
        for (size_t i = 0; i < vertices.size(); i++)
        {
            const float weights[4] = { source[i].BoneWeights.x, source[i].BoneWeights.y, source[i].BoneWeights.z, source[i].BoneWeights.w };
            const uint32_t joints[4] = { source[i].BoneIndices.x, source[i].BoneIndices.y, source[i].BoneIndices.z, source[i].BoneIndices.w };
            int largest = static_cast<int>(std::max_element(weights, weights + 4) - weights);
            vertices[i] = { source[i].Position, source[i].Normal, source[i].TexCoord, weights[largest] > 0.0f ? static_cast<int32_t>(joints[largest]) : -1 };
        }
    }
    else
    {
        const RHIStructures::Vertex* source = static_cast<const RHIStructures::Vertex*>(mesh.GetCachedVertexData());
        for (size_t i = 0; i < vertices.size(); i++)
            vertices[i] = { source[i].Position, source[i].Normal, source[i].TexCoord, -1 };
    }
    mesh.SetChart(ChartBuilder::Build(vertices, { indices, mesh.GetIndexCount() }, mesh.IsSkinned() ? state.ModelSkeleton : nullptr,
                                      state.MillimetresPerUnit));
    state.ChartMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (!mesh.GetChart().Map.empty())
        std::cout << "Chart " << mesh.GetMaterialName() << ": " << mesh.GetChart().MillimetresU << " x " << mesh.GetChart().MillimetresV
                  << " mm per UV unit, chart map " << MeshChart::MapSize << " x " << MeshChart::MapSize << std::endl;
}

static void CollectModelTransforms(const aiNode* node, const XMMATRIX& model, ImportState& state)
{
    XMStoreFloat4x4(&state.ModelByNode[node], model);
    for (uint32_t i = 0; i < node->mNumChildren; i++)
        CollectModelTransforms(node->mChildren[i], ToMatrix(node->mChildren[i]->mTransformation) * model, state);
}

static void CollectJoints(const aiNode* node, int32_t parentJoint, const std::unordered_map<const aiNode*, bool>& isMarked,
                          std::vector<Joint>& joints, ImportState& state)
{
    int32_t joint = parentJoint;
    if (isMarked.contains(node))
    {
        Joint newJoint;
        newJoint.Name = node->mName.C_Str();
        newJoint.Parent = parentJoint;
        XMVECTOR scale, rotation, translation;
        XMMatrixDecompose(&scale, &rotation, &translation, ToMatrix(node->mTransformation));
        XMStoreFloat3(&newJoint.BindLocal.Scale, scale);
        XMStoreFloat4(&newJoint.BindLocal.Rotation, rotation);
        XMStoreFloat3(&newJoint.BindLocal.Translation, translation);
        XMStoreFloat4x4(&newJoint.InverseBind, XMMatrixInverse(nullptr, state.GetModel(node)));

        joint = static_cast<int32_t>(joints.size());
        state.JointByNode[node] = joint;
        state.JointByName[newJoint.Name] = joint;
        joints.push_back(newJoint);
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++)
        CollectJoints(node->mChildren[i], joint, isMarked, joints, state);
}

static void CollectSkinnedMeshes(const aiNode* node, const aiScene* scene, std::vector<std::pair<const aiNode*, const aiMesh*>>& out)
{
    for (uint32_t i = 0; i < node->mNumMeshes; i++)
        if (scene->mMeshes[node->mMeshes[i]]->mNumBones > 0)
            out.push_back({ node, scene->mMeshes[node->mMeshes[i]] });
    for (uint32_t i = 0; i < node->mNumChildren; i++)
        CollectSkinnedMeshes(node->mChildren[i], scene, out);
}

// The joints are every bone node and every ancestor of one below the model root, in parent-first
// order. A joint's inverse bind comes from the meshes' bone offsets, moved into model space; a joint
// no mesh binds to uses the inverse of its node's model transform.
static void BuildSkeleton(ImportState& state)
{
    const aiScene* scene = state.Scene;
    std::vector<std::pair<const aiNode*, const aiMesh*>> skinnedMeshes;
    CollectSkinnedMeshes(scene->mRootNode, scene, skinnedMeshes);
    if (skinnedMeshes.empty())
        return;

    std::unordered_map<const aiNode*, bool> isMarked;
    for (const auto& [node, mesh] : skinnedMeshes)
        for (uint32_t i = 0; i < mesh->mNumBones; i++)
        {
            const aiNode* boneNode = scene->mRootNode->FindNode(mesh->mBones[i]->mName);
            if (!boneNode)
            {
                std::cout << "Import warning: bone " << mesh->mBones[i]->mName.C_Str() << " has no node in " << state.Name << std::endl;
                continue;
            }
            for (const aiNode* ancestor = boneNode; ancestor && ancestor != scene->mRootNode; ancestor = ancestor->mParent)
                isMarked[ancestor] = true;
        }

    std::vector<Joint> joints;
    CollectJoints(scene->mRootNode, -1, isMarked, joints, state);

    std::vector<bool> isBound(joints.size(), false);
    for (const auto& [node, mesh] : skinnedMeshes)
    {
        XMMATRIX meshToModel = state.GetModel(node);
        XMMATRIX modelToMesh = XMMatrixInverse(nullptr, meshToModel);
        for (uint32_t i = 0; i < mesh->mNumBones; i++)
        {
            const aiBone* bone = mesh->mBones[i];
            auto found = state.JointByName.find(bone->mName.C_Str());
            if (found == state.JointByName.end())
                continue;

            Joint& joint = joints[found->second];
            XMFLOAT4X4 inverseBind;
            XMStoreFloat4x4(&inverseBind, modelToMesh * ToMatrix(bone->mOffsetMatrix));
            if (isBound[found->second] && !IsNearlyEqual(joint.InverseBind, inverseBind))
                std::cout << "Import warning: meshes disagree about the inverse bind of joint " << joint.Name << " in " << state.Name << std::endl;
            joint.InverseBind = inverseBind;
            isBound[found->second] = true;

            XMVECTOR jointPosition = XMMatrixInverse(nullptr, XMLoadFloat4x4(&inverseBind)).r[3];
            for (uint32_t w = 0; w < bone->mNumWeights; w++)
            {
                if (bone->mWeights[w].mWeight <= 0.0f)
                    continue;
                const aiVector3D& p = mesh->mVertices[bone->mWeights[w].mVertexId];
                XMVECTOR position = XMVector3TransformCoord(XMVectorSet(p.x, p.y, p.z, 1.0f), meshToModel);
                joint.InfluenceRadius = (std::max)(joint.InfluenceRadius, XMVectorGetX(XMVector3Length(position - jointPosition)));
            }
        }
    }

    state.ModelSkeleton = new Skeleton(state.Name + "_skeleton", std::move(joints));
    ResourceManager::CreateAsset(state.ModelSkeleton, ResourceType::Skeleton, state.DestDir);
}

// One clip per animation, its channels matched to joints by name.
static std::vector<std::pair<std::string, Clip*>> LoadClips(const ImportState& state)
{
    std::vector<std::pair<std::string, Clip*>> clips;
    const aiScene* scene = state.Scene;
    for (uint32_t a = 0; a < scene->mNumAnimations; a++)
    {
        const aiAnimation* animation = scene->mAnimations[a];
        double ticksPerSecond = animation->mTicksPerSecond > 0.0 ? animation->mTicksPerSecond : 25.0;
        std::vector<JointTrack> tracks(state.ModelSkeleton->GetJointCount());

        for (uint32_t c = 0; c < animation->mNumChannels; c++)
        {
            const aiNodeAnim* channel = animation->mChannels[c];
            auto found = state.JointByName.find(channel->mNodeName.C_Str());
            if (found == state.JointByName.end())
            {
                std::cout << "Import warning: animation channel " << channel->mNodeName.C_Str() << " matches no joint in " << state.Name << std::endl;
                continue;
            }

            JointTrack& track = tracks[found->second];
            for (uint32_t k = 0; k < channel->mNumPositionKeys; k++)
            {
                const aiVectorKey& key = channel->mPositionKeys[k];
                track.Translations.push_back({ static_cast<float>(key.mTime / ticksPerSecond), { key.mValue.x, key.mValue.y, key.mValue.z } });
            }
            for (uint32_t k = 0; k < channel->mNumRotationKeys; k++)
            {
                const aiQuatKey& key = channel->mRotationKeys[k];
                track.Rotations.push_back({ static_cast<float>(key.mTime / ticksPerSecond), { key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w } });
            }
            for (uint32_t k = 0; k < channel->mNumScalingKeys; k++)
            {
                const aiVectorKey& key = channel->mScalingKeys[k];
                track.Scales.push_back({ static_cast<float>(key.mTime / ticksPerSecond), { key.mValue.x, key.mValue.y, key.mValue.z } });
            }
        }

        std::string clipName = animation->mName.length ? animation->mName.C_Str() : "Clip " + std::to_string(a);
        std::string assetName = state.Name + "_" + clipName;
        std::replace_if(assetName.begin(), assetName.end(), [](char c) { return c == '|' || c == '/' || c == '\\' || c == ':' || c == ' '; }, '_');
        Clip* clip = new Clip(assetName, static_cast<float>(animation->mDuration / ticksPerSecond), std::move(tracks));
        ResourceManager::CreateAsset(clip, ResourceType::AnimationClip, state.DestDir);
        clips.push_back({ clipName, clip });
    }
    return clips;
}

// Joints and the nodes below them become no scene nodes; any of them with meshes becomes one under
// the model root, at its bind-pose model transform.
static SceneNode* LoadNode(const aiNode* node, const XMMATRIX& local, SceneNode* parent, bool isBelowJoint, ImportState& state)
{
    const aiScene* scene = state.Scene;
    bool isDetached = isBelowJoint || state.IsJoint(node);
    std::string nodeName = node->mName.length ? std::string(node->mName.C_Str()) : state.Name;

    std::vector<Mesh> meshes;
    meshes.reserve(node->mNumMeshes);
    for (uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        const aiMesh* assimpMesh = scene->mMeshes[node->mMeshes[i]];
        if (state.ModelSkeleton && assimpMesh->mNumBones > 0)
            meshes.emplace_back(GeometryImport::LoadSkinnedMesh(assimpMesh, state.GetModel(node), state.JointByName));
        else
        {
            if (isDetached)
                std::cout << "Import warning: mesh below joint " << nodeName << " is attached to the model root at its bind position" << std::endl;
            meshes.emplace_back(GeometryImport::LoadMesh(assimpMesh));
        }
        meshes.back().SetMaterialName(scene->mMaterials[assimpMesh->mMaterialIndex]->GetName().C_Str());
        MeasureChart(meshes.back(), state);
    }

    SceneNode* sceneNode = nullptr;
    if (!isDetached || !meshes.empty())
        sceneNode = new SceneNode(nodeName, isDetached ? state.GetModel(node) : local, isDetached ? state.Root : parent);
    if (!state.Root)
        state.Root = sceneNode;

    if (!meshes.empty())
    {
        MeshComponent* meshComponent = static_cast<MeshComponent*>(sceneNode->AddComponent(MeshComponentType));
        MeshAsset* meshAsset = new MeshAsset(meshes, nodeName);
        bool isSkinned = false;
        for (uint32_t i = 0; i < meshAsset->GetMeshCount(); i++)
            isSkinned |= meshAsset->GetMesh(i).IsSkinned();
        if (isSkinned)
            meshAsset->SetSkeleton(state.ModelSkeleton);
        // Register the asset before wiring it onto the component so Field::SetID's type check passes.
        ResourceManager::CreateAsset(meshAsset, ResourceType::Mesh, state.DestDir);
        meshComponent->SetMeshAsset(meshAsset);
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++)
    {
        const aiNode* child = node->mChildren[i];
        if (isDetached)
            LoadNode(child, state.GetModel(child), state.Root, true, state);
        else
            LoadNode(child, ToMatrix(child->mTransformation), sceneNode, false, state);
    }
    return sceneNode;
}

static void ReadVertexBasis(const aiMesh* mesh, size_t i, RHIStructures::Vertex& vertex)
{
    vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
    if (mesh->mTextureCoords[0])
        vertex.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
    vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
    vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
    vertex.Bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
}

static void ReadIndices(const aiMesh* mesh, std::vector<uint32_t>& indices)
{
    indices.reserve(mesh->mNumFaces * 3);
    for (uint32_t i = 0; i < mesh->mNumFaces; i++)
        for (uint32_t j = 0; j < mesh->mFaces[i].mNumIndices; j++)
            indices.push_back(mesh->mFaces[i].mIndices[j]);
}

Mesh GeometryImport::LoadMesh(const aiMesh* mesh)
{
    VertexCache* vertexCache = new VertexCache();
    vertexCache->Vertices.resize(mesh->mNumVertices);
    for (size_t i = 0; i < mesh->mNumVertices; i++)
        ReadVertexBasis(mesh, i, vertexCache->Vertices[i]);
    ReadIndices(mesh, vertexCache->Indices);
    return Mesh(vertexCache, mesh->mMaterialIndex);
}

Mesh GeometryImport::LoadSkinnedMesh(const aiMesh* mesh, const XMMATRIX& meshToModel, const std::unordered_map<std::string, int32_t>& jointByName)
{
    using namespace RHIStructures;

    SkinnedVertexCache* cache = new SkinnedVertexCache();
    cache->Vertices.resize(mesh->mNumVertices);

    // Vertices are baked into model space, so one palette serves every mesh of the model.
    XMMATRIX normalMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, meshToModel));
    for (size_t i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex basis;
        ReadVertexBasis(mesh, i, basis);
        SkinnedVertex& vertex = cache->Vertices[i];
        XMStoreFloat3(&vertex.Position, XMVector3TransformCoord(XMLoadFloat3(&basis.Position), meshToModel));
        XMStoreFloat3(&vertex.Normal, XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&basis.Normal), normalMatrix)));
        XMStoreFloat3(&vertex.Tangent, XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&basis.Tangent), meshToModel)));
        XMStoreFloat3(&vertex.Bitangent, XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&basis.Bitangent), meshToModel)));
        vertex.TexCoord = basis.TexCoord;
        vertex.TexCoord1 = basis.TexCoord1;
        vertex.PatchWeight = basis.PatchWeight;
        vertex.PatchChart = basis.PatchChart;
        vertex.BoneWeights = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        vertex.BoneIndices = XMUINT4(0, 0, 0, 0);
    }

    // Bone to vertices becomes vertex to joints; the four largest weights are kept.
    std::vector<std::vector<std::pair<uint32_t, float>>> influences(mesh->mNumVertices);
    for (uint32_t b = 0; b < mesh->mNumBones; b++)
    {
        const aiBone* bone = mesh->mBones[b];
        auto found = jointByName.find(bone->mName.C_Str());
        if (found == jointByName.end())
            continue;
        for (uint32_t w = 0; w < bone->mNumWeights; w++)
            influences[bone->mWeights[w].mVertexId].push_back({ static_cast<uint32_t>(found->second), bone->mWeights[w].mWeight });
    }

    for (size_t v = 0; v < mesh->mNumVertices; v++)
    {
        std::vector<std::pair<uint32_t, float>>& list = influences[v];
        std::sort(list.begin(), list.end(), [](const std::pair<uint32_t, float>& a, const std::pair<uint32_t, float>& b) { return a.second > b.second; });

        float weights[4] = {};
        uint32_t joints[4] = {};
        float weightSum = 0.0f;
        for (size_t i = 0; i < (std::min)(list.size(), size_t(4)); i++)
        {
            joints[i] = list[i].first;
            weights[i] = list[i].second;
            weightSum += weights[i];
        }
        if (weightSum > 0.0f)
            for (float& weight : weights)
                weight /= weightSum;

        cache->Vertices[v].BoneWeights = XMFLOAT4(weights[0], weights[1], weights[2], weights[3]);
        cache->Vertices[v].BoneIndices = XMUINT4(joints[0], joints[1], joints[2], joints[3]);
    }

    ReadIndices(mesh, cache->Indices);
    return Mesh(cache, mesh->mMaterialIndex);
}

SceneNode* GeometryImport::CreateMeshNode(const std::string& name, VertexCache* vertices, const std::string& assetDirectory)
{
    std::vector<Mesh> meshes;
    meshes.emplace_back(vertices, 0);
    MeshAsset* meshAsset = new MeshAsset(meshes, name);
    std::filesystem::create_directories(assetDirectory);
    ResourceManager::CreateAsset(meshAsset, ResourceType::Mesh, assetDirectory);

    SceneNode* node = new SceneNode(name, XMMatrixIdentity());
    MeshComponent* component = static_cast<MeshComponent*>(node->AddComponent(MeshComponentType));
    component->SetMeshAsset(meshAsset);
    return node;
}

SceneNode* GeometryImport::CreateMeshGroup(const std::string& sourcePath, const std::string& name, const XMMATRIX& transform, bool allowSkinned)
{
    Assimp::Importer importer;
    // Without this Assimp splits each FBX joint into helper nodes and clip channels stop naming joints.
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    const aiScene* scene = importer.ReadFile(sourcePath,
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
        aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals | aiProcess_ConvertToLeftHanded);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        throw std::runtime_error("Failed to load model " + sourcePath + ": " + importer.GetErrorString());

    ImportState state;
    state.Scene = scene;
    state.Name = name;
    state.DestDir = std::filesystem::path(sourcePath).parent_path().string();

    // FBX states its unit as centimetres per unit; Assimp leaves positions in it.
    double unitScale = 1.0;
    float unitScaleFloat = 1.0f;
    if (scene->mMetaData && !scene->mMetaData->Get("UnitScaleFactor", unitScale) && scene->mMetaData->Get("UnitScaleFactor", unitScaleFloat))
        unitScale = unitScaleFloat;
    state.MillimetresPerUnit = static_cast<float>(unitScale * 10.0);
    CollectModelTransforms(scene->mRootNode, XMMatrixIdentity(), state);
    if (allowSkinned)
        BuildSkeleton(state);

    SceneNode* root = LoadNode(scene->mRootNode, ToMatrix(scene->mRootNode->mTransformation) * transform, nullptr, false, state);
    std::cout << "Import " << name << ": charts measured in " << state.ChartMilliseconds << " ms" << std::endl;

    if (state.ModelSkeleton)
    {
        AnimatorComponent* animator = static_cast<AnimatorComponent*>(root->AddComponent(AnimatorComponentType));
        animator->SetSkeleton(state.ModelSkeleton);
        for (const auto& [clipName, clip] : LoadClips(state))
            animator->AddState({ clipName, clip });
    }
    return root;
}
