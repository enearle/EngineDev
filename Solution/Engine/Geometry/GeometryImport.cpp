#include "GeometryImport.h"

#include <iostream>
#include <stdexcept>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include "Mesh.h"
#include "DirectXMath.h"
#include "../RHI/RHI/RHIStructures.h"

using namespace DirectX;

MeshNode* GeometryImport::LoadNode(aiNode* node, const aiScene* scene, const XMMATRIX& transform, const std::string name, 
    bool allowSkinned, SceneNode* parent)
{
    XMMATRIX newTransform = XMMatrixTranspose(XMMATRIX(&node->mTransformation.a1)) * transform;
    std::vector<Mesh> meshes(node->mNumMeshes);
    uint32_t numMats = 0;
    for (size_t i = 0; i < node->mNumMeshes; i++)
    {
        uint32_t meshIndex = node->mMeshes[i];
        aiMesh* assimpMesh = scene->mMeshes[meshIndex];
        if (allowSkinned && assimpMesh->mNumBones > 0)
            meshes[i] = LoadSkinnedMesh(assimpMesh, XMMATRIX(&node->mTransformation.a1) * transform);
        else
            meshes[i] = LoadMesh(assimpMesh, newTransform);
    }
    
    //////////////////////////////////////////////////////////////////////////
    // TODO: Convert to load SceneNode with MeshComponent that has a MeshAsset
    //////////////////////////////////////////////////////////////////////////
    ///
    SceneNode* newSceneNode = new SceneNode(name, newTransform, parent);
    MeshComponent* meshComponent = static_cast<MeshComponent*>(newSceneNode->AddComponent(MeshComponentType));
    if (meshComponent)
    {
        MeshAsset* meshAsset = new MeshAsset(meshes, name);
        meshComponent->SetMeshAsset(meshAsset);
        meshAsset->ResourceManager->
    }
    
    MeshNode* newNode = new MeshNode(meshes, newTransform, name, parent);
    for (size_t i = 0; i < node->mNumChildren; i++)
        newNode->AddChild(LoadNode(node->mChildren[i], scene, newTransform, name, allowSkinned, newNode));
    return newNode;
}

Mesh GeometryImport::LoadMesh(aiMesh* mesh, const XMMATRIX& transform)
{

    VertexCache* vertexCache = new VertexCache();
    
    vertexCache->Vertices.resize(mesh->mNumVertices);
    vertexCache->Indices.reserve(mesh->mNumFaces * 3);
    
    for (size_t i = 0; i < mesh->mNumVertices; i++)
    {
        vertexCache->Vertices[i].Position.x = mesh->mVertices[i].x;
        vertexCache->Vertices[i].Position.y = mesh->mVertices[i].y;
        vertexCache->Vertices[i].Position.z = mesh->mVertices[i].z;
        
        if (mesh->mTextureCoords[0])
        {
            vertexCache->Vertices[i].TexCoord.x = mesh->mTextureCoords[0][i].x;
            vertexCache->Vertices[i].TexCoord.y = mesh->mTextureCoords[0][i].y;
        }

        vertexCache->Vertices[i].Normal.x = mesh->mNormals[i].x;
        vertexCache->Vertices[i].Normal.y = mesh->mNormals[i].y;
        vertexCache->Vertices[i].Normal.z = mesh->mNormals[i].z;

        vertexCache->Vertices[i].Tangent.x = mesh->mTangents[i].x;
        vertexCache->Vertices[i].Tangent.y = mesh->mTangents[i].y;
        vertexCache->Vertices[i].Tangent.z = mesh->mTangents[i].z;

        vertexCache->Vertices[i].Bitangent.x = mesh->mBitangents[i].x;
        vertexCache->Vertices[i].Bitangent.y = mesh->mBitangents[i].y;
        vertexCache->Vertices[i].Bitangent.z = mesh->mBitangents[i].z;
    }
    
    for (size_t i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (size_t j = 0; j < face.mNumIndices; j++)
            vertexCache->Indices.push_back(face.mIndices[j]);
    }
    
    return Mesh(vertexCache, mesh->mMaterialIndex);
}

Mesh GeometryImport::LoadSkinnedMesh(aiMesh* mesh, const XMMATRIX& transform)
{
    using namespace RHIStructures;
    
    SkinnedVertexCache* skinnedVertexCache = new SkinnedVertexCache();
    
    skinnedVertexCache->Vertices.resize(mesh->mNumVertices);
    skinnedVertexCache->Indices.reserve(mesh->mNumFaces * 3);
    
    for (size_t i = 0; i < mesh->mNumVertices; i++)
    {
        skinnedVertexCache->Vertices[i].Position.x = mesh->mVertices[i].x;
        skinnedVertexCache->Vertices[i].Position.y = mesh->mVertices[i].y;
        skinnedVertexCache->Vertices[i].Position.z = mesh->mVertices[i].z;
        
        if (mesh->mTextureCoords[0])
        {
            skinnedVertexCache->Vertices[i].TexCoord.x = mesh->mTextureCoords[0][i].x;
            skinnedVertexCache->Vertices[i].TexCoord.y = mesh->mTextureCoords[0][i].y;
        }

        skinnedVertexCache->Vertices[i].Normal.x = mesh->mNormals[i].x;
        skinnedVertexCache->Vertices[i].Normal.y = mesh->mNormals[i].y;
        skinnedVertexCache->Vertices[i].Normal.z = mesh->mNormals[i].z;

        skinnedVertexCache->Vertices[i].Tangent.x = mesh->mTangents[i].x;
        skinnedVertexCache->Vertices[i].Tangent.y = mesh->mTangents[i].y;
        skinnedVertexCache->Vertices[i].Tangent.z = mesh->mTangents[i].z;

        skinnedVertexCache->Vertices[i].Bitangent.x = mesh->mBitangents[i].x;
        skinnedVertexCache->Vertices[i].Bitangent.y = mesh->mBitangents[i].y;
        skinnedVertexCache->Vertices[i].Bitangent.z = mesh->mBitangents[i].z;
        
        skinnedVertexCache->Vertices[i].BoneWeights = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        skinnedVertexCache->Vertices[i].BoneIndices = XMUINT4(0, 0, 0, 0);
    }
    
    std::vector<XMMATRIX> boneOffsets;
    std::vector<XMMATRIX> boneTransforms;
    boneOffsets.resize(mesh->mNumBones);
    boneTransforms.resize(mesh->mNumBones);

    // Convert from bone->vertices mapping to vertex->bones mapping and store inverse bind pose matrices
    std::vector<std::vector<std::pair<uint32_t, float>>> vertexBoneData(mesh->mNumVertices);
    
    for (size_t boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++)
    {
        aiBone* bone = mesh->mBones[boneIndex];
        
        aiMatrix4x4 offsetMatrix = bone->mOffsetMatrix;
        boneOffsets[boneIndex] = XMMATRIX(&offsetMatrix.a1);
        
        for (size_t weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++)
        {
            aiVertexWeight weight = bone->mWeights[weightIndex];
            uint32_t vertexId = weight.mVertexId;
            float boneWeight = weight.mWeight;
            
            vertexBoneData[vertexId].push_back(std::make_pair(boneIndex, boneWeight));
        }
    }
    
    // Assign bone weights and indices to each vertex
    for (size_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; vertexIndex++)
    {
        auto& boneData = vertexBoneData[vertexIndex];
        
        // Sort bone weights and take 4 most influential bones
        std::sort(boneData.begin(), boneData.end(), 
            [](const std::pair<uint32_t, float>& a, const std::pair<uint32_t, float>& b) {
                return a.second > b.second;
            });
        
        float weightSum = 0.0f;
        size_t numBones = std::min(boneData.size(), size_t(4));
        
        float weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        uint32_t indices[4] = {0, 0, 0, 0};
        
        for (size_t i = 0; i < numBones; i++)
        {
            indices[i] = boneData[i].first;
            weights[i] = boneData[i].second;
            weightSum += weights[i];
        }
        
        // Normalize weights to sum to 1.0
        if (weightSum > 0.0f)
        {
            for (size_t i = 0; i < 4; i++)
                weights[i] /= weightSum;
        }
        
        skinnedVertexCache->Vertices[vertexIndex].BoneWeights = XMFLOAT4(weights[0], weights[1], weights[2], weights[3]);
        skinnedVertexCache->Vertices[vertexIndex].BoneIndices = XMUINT4(indices[0], indices[1], indices[2], indices[3]);
    }
    
    for (size_t i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (size_t j = 0; j < face.mNumIndices; j++)
            skinnedVertexCache->Indices.push_back(face.mIndices[j]);
    }
    
    return Mesh(skinnedVertexCache, mesh->mMaterialIndex, boneOffsets, boneTransforms);
}

MeshNode* GeometryImport::CreateMeshGroup(std::string filePath, const std::string& name, const XMMATRIX& transform, bool allowSkinned)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile("../Engine/Meshes/" + filePath, 
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | 
        aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals | aiProcess_ConvertToLeftHanded);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        throw std::runtime_error("Failed to load model: " + filePath);
    
    return LoadNode(scene->mRootNode, scene, transform, name, allowSkinned);
}