#pragma once
#include "Mesh.h"
#include "../RenderPassExecutor.h"


struct Vertex
{
    DirectX::XMFLOAT3 Position = {0,0,0};
    DirectX::XMFLOAT3 Normal = {0,0,0};
    DirectX::XMFLOAT3 Tangent = {0,0,0};
    DirectX::XMFLOAT3 Bitangent = {0,0,0};
    DirectX::XMFLOAT2 TexCoord = {0,0};
};


class GeometryImport
{
public:
    static SceneNode LoadNode(aiNode* node, const aiScene* scene, const DirectX::XMMATRIX& transform);
    static Mesh LoadMesh(aiMesh* mesh, const DirectX::XMMATRIX& transform);
    static RootNode CreateMeshGroup(std::string filePath, const std::string& name, const DirectX::XMMATRIX& transform);
};
