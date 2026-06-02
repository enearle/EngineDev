#pragma once
#include "Mesh.h"
#include "../ENGINE_API_Macro.h"
struct aiNode;
struct aiScene;
struct aiMesh;

class SceneNode;

class ENGINE_API GeometryImport
{
public:
    static SceneNode* LoadNode(aiNode* node, const aiScene* scene, const DirectX::XMMATRIX& transform, const std::string& name, bool allowSkinned, const std::string& destDir, SceneNode* parent = nullptr);
    static Mesh LoadMesh(aiMesh* mesh, const DirectX::XMMATRIX& transform);
    static Mesh LoadSkinnedMesh(aiMesh* mesh, const DirectX::XMMATRIX& transform);
    static SceneNode* CreateMeshGroup(const std::string& sourcePath, const std::string& name, const DirectX::XMMATRIX& transform, bool allowSkinned = true);
};
