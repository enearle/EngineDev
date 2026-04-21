#pragma once
#include "Mesh.h"
#include "../PipelineExecutor.h"
#include "../../RHI_API_Macro.h"
struct aiNode;
struct aiScene;
struct aiMesh;
class RHI_API GeometryImport
{
public:
    static SceneNode LoadNode(aiNode* node, const aiScene* scene, const DirectX::XMMATRIX& transform, bool allowSkinned);
    static Mesh LoadMesh(aiMesh* mesh, const DirectX::XMMATRIX& transform);
    static Mesh LoadSkinnedMesh(aiMesh* mesh, const DirectX::XMMATRIX& transform);
    static RootNode CreateMeshGroup(std::string filePath, const std::string& name, const DirectX::XMMATRIX& transform, bool allowSkinned = true);
};
