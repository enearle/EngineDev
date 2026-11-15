#pragma once
#include <string>
#include <unordered_map>
#include "Mesh.h"
#include "../ENGINE_API_Macro.h"
struct aiMesh;

class SceneNode;

class ENGINE_API GeometryImport
{
public:
    static Mesh LoadMesh(const aiMesh* mesh);
    // Vertices are moved into model space and bone indices become joint indices.
    static Mesh LoadSkinnedMesh(const aiMesh* mesh, const DirectX::XMMATRIX& meshToModel, const std::unordered_map<std::string, int32_t>& jointByName);
    // A skinned model gets a skeleton, an AnimatorComponent on the returned root and one state per clip in the file.
    static SceneNode* CreateMeshGroup(const std::string& sourcePath, const std::string& name, const DirectX::XMMATRIX& transform, bool allowSkinned = true);
};
