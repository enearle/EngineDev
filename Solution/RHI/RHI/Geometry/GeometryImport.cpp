#include "GeometryImport.h"

#include <iostream>
#include <stdexcept>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include "Mesh.h"
#include "DirectXMath.h"
#include "../RHIStructures.h"

using namespace DirectX;

SceneNode GeometryImport::LoadNode(aiNode* node, const aiScene* scene, const XMMATRIX& transform, bool allowSkinned)
{
    XMMATRIX newTransform = XMMatrixTranspose(XMMATRIX(&node->mTransformation.a1)) * transform;
    SceneNode newNode;
    newNode.SetModelMatrix(newTransform);
    for (size_t i = 0; i < node->mNumMeshes; i++)
    {
        uint32_t meshIndex = node->mMeshes[i];
        aiMesh* assimpMesh = scene->mMeshes[meshIndex];
        
        Mesh mesh;
        if (allowSkinned && assimpMesh->mNumBones > 0)
            mesh = LoadSkinnedMesh(assimpMesh, XMMATRIX(&node->mTransformation.a1) * transform);
        else
            mesh = LoadMesh(assimpMesh, newTransform);
        
        newNode.AddMesh(mesh);
    }
    
    for (size_t i = 0; i < node->mNumChildren; i++)
        newNode.AddChild(LoadNode(node->mChildren[i], scene, +newTransform, allowSkinned));
    
    return newNode;
}

Mesh GeometryImport::LoadMesh(aiMesh* mesh, const XMMATRIX& transform)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    vertices.resize(mesh->mNumVertices);
    indices.reserve(mesh->mNumFaces * 3);
    
    for (size_t i = 0; i < mesh->mNumVertices; i++)
    {
        vertices[i].Position.x = mesh->mVertices[i].x;
        vertices[i].Position.y = mesh->mVertices[i].y;
        vertices[i].Position.z = mesh->mVertices[i].z;
        
        if (mesh->mTextureCoords[0])
        {
            vertices[i].TexCoord.x = mesh->mTextureCoords[0][i].x;
            vertices[i].TexCoord.y = mesh->mTextureCoords[0][i].y;
        }

        vertices[i].Normal.x = mesh->mNormals[i].x;
        vertices[i].Normal.y = mesh->mNormals[i].y;
        vertices[i].Normal.z = mesh->mNormals[i].z;

        vertices[i].Tangent.x = mesh->mTangents[i].x;
        vertices[i].Tangent.y = mesh->mTangents[i].y;
        vertices[i].Tangent.z = mesh->mTangents[i].z;

        vertices[i].Bitangent.x = mesh->mBitangents[i].x;
        vertices[i].Bitangent.y = mesh->mBitangents[i].y;
        vertices[i].Bitangent.z = mesh->mBitangents[i].z;
    }
    
    for (size_t i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (size_t j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }
    
    return Mesh(&vertices, &indices, mesh->mMaterialIndex);
}

Mesh GeometryImport::LoadSkinnedMesh(aiMesh* mesh, const XMMATRIX& transform)
{
    using namespace RHIStructures;
    
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32_t> indices;
    
    vertices.resize(mesh->mNumVertices);
    indices.reserve(mesh->mNumFaces * 3);
    
    for (size_t i = 0; i < mesh->mNumVertices; i++)
    {
        vertices[i].Position.x = mesh->mVertices[i].x;
        vertices[i].Position.y = mesh->mVertices[i].y;
        vertices[i].Position.z = mesh->mVertices[i].z;
        
        if (mesh->mTextureCoords[0])
        {
            vertices[i].TexCoord.x = mesh->mTextureCoords[0][i].x;
            vertices[i].TexCoord.y = mesh->mTextureCoords[0][i].y;
        }

        vertices[i].Normal.x = mesh->mNormals[i].x;
        vertices[i].Normal.y = mesh->mNormals[i].y;
        vertices[i].Normal.z = mesh->mNormals[i].z;

        vertices[i].Tangent.x = mesh->mTangents[i].x;
        vertices[i].Tangent.y = mesh->mTangents[i].y;
        vertices[i].Tangent.z = mesh->mTangents[i].z;

        vertices[i].Bitangent.x = mesh->mBitangents[i].x;
        vertices[i].Bitangent.y = mesh->mBitangents[i].y;
        vertices[i].Bitangent.z = mesh->mBitangents[i].z;
        
        vertices[i].BoneWeights = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        vertices[i].BoneIndices = XMUINT4(0, 0, 0, 0);
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
        
        vertices[vertexIndex].BoneWeights = XMFLOAT4(weights[0], weights[1], weights[2], weights[3]);
        vertices[vertexIndex].BoneIndices = XMUINT4(indices[0], indices[1], indices[2], indices[3]);
    }
    
    for (size_t i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (size_t j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }
    
    return Mesh(&vertices, &indices, mesh->mMaterialIndex, boneOffsets, boneTransforms);
}

RootNode GeometryImport::CreateMeshGroup(std::string filePath, const std::string& name, const XMMATRIX& transform, bool allowSkinned)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile("../Engine/Meshes/" + filePath, 
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | 
        aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals | aiProcess_ConvertToLeftHanded);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        throw std::runtime_error("Failed to load model: " + filePath);
    
    return RootNode(LoadNode(scene->mRootNode, scene, transform, allowSkinned), scene->mNumMaterials);
}