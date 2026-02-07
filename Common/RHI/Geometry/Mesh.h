#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLFW_INCLUDE_VULKAN

#include <DirectXMath.h>
#include <string>
#include <vector>
#include <assimp/scene.h>

struct aiScene;
struct DirectX::XMMATRIX;
struct Vertex;

class Mesh
{
public:
    
    Mesh();
    Mesh(std::vector<Vertex>* vertices, std::vector<uint32_t>* indices, uint32_t LocalMaterialIndex);
    ~Mesh();

    uint32_t GetVertexCount() const                     { return VertexCount; }
    uint32_t GetIndexCount() const                      { return IndexCount; }
    uint32_t GetLocalMaterialIndex() const              { return LocalMaterialIndex; }

private:
    
    uint32_t VertexCount;
    uint32_t IndexCount;
    uint32_t LocalMaterialIndex;
    
};

struct SceneNode
{
    SceneNode() = default;
    SceneNode(std::vector<Mesh>&& meshes, const DirectX::XMMATRIX modelMatrix, const std::string& name, uint32_t numMaterials) : Name(name), Meshes(std::move(meshes)), Model(modelMatrix), NumMaterials(numMaterials) {}
    ~SceneNode() = default;

    std::string Name;
    void AddChild(SceneNode child)                      { Children.push_back(std::move(child)); }
    void AddMesh(Mesh mesh)                             { Meshes.push_back(std::move(mesh)); }
    std::vector<SceneNode> GetChildren() const          { return Children; }
    std::vector<Mesh> GetMeshes() const                 { return Meshes; }
    DirectX::XMMATRIX GetModelMatrix() const            { return Model; }
    void SetModelMatrix(DirectX::XMMATRIX modelMatrix)  { Model = modelMatrix; }
    size_t GetMeshCount() const                         { return Meshes.size(); }
    uint32_t GetNumMaterials() const                    { return NumMaterials; }
    const Mesh* GetMesh(uint32_t index) const;
    
private:
    
    uint32_t NumMaterials;
    std::vector<Mesh> Meshes;
    std::vector<SceneNode> Children;
    DirectX::XMMATRIX Model;
};

struct RootNode
{
    RootNode() = default;
    RootNode(SceneNode&& sceneNode, uint32_t numMaterials) : SceneNode(std::move(sceneNode)), NumMaterials(numMaterials) {}
    ~RootNode() = default;
    
    SceneNode& GetSceneNode()                           { return SceneNode; }
    uint32_t GetNumMaterials() const                    { return NumMaterials; }   

private:
    
    SceneNode SceneNode;
    uint32_t NumMaterials = 0;
};