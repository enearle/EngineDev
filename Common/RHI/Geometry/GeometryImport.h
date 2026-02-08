#pragma once
#include "Mesh.h"
#include "../RenderPassExecutor.h"

class GeometryImport
{
public:
    static SceneNode LoadNode(aiNode* node, const aiScene* scene, const DirectX::XMMATRIX& transform);
    static Mesh LoadMesh(aiMesh* mesh, const DirectX::XMMATRIX& transform);
    static RootNode CreateMeshGroup(std::string filePath, const std::string& name, const DirectX::XMMATRIX& transform);
};
