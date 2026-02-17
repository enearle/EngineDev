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

SceneNode GeometryImport::LoadNode(aiNode* node, const aiScene* scene, const XMMATRIX& transform)
{
    XMMATRIX newTransform = XMMatrixTranspose(XMMATRIX(&node->mTransformation.a1)) * transform;
    SceneNode newNode;
    newNode.SetModelMatrix(newTransform);
    for (size_t i = 0; i < node->mNumMeshes; i++)
    {
        uint32_t meshIndex = node->mMeshes[i];
        Mesh mesh = LoadMesh(scene->mMeshes[meshIndex], newTransform);
        newNode.AddMesh(mesh);
    }
    
    for (size_t i = 0; i < node->mNumChildren; i++)
        newNode.AddChild(LoadNode(node->mChildren[i], scene, +newTransform));
    
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

RootNode GeometryImport::CreateMeshGroup(std::string filePath, const std::string& name, const XMMATRIX& transform)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile("Meshes/" + filePath, 
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | 
        aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        throw std::runtime_error("Failed to load model: " + filePath);
    
    return RootNode(LoadNode(scene->mRootNode, scene, transform), scene->mNumMaterials);
}