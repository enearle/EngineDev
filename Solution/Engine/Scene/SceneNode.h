#pragma once
#include <string>
#include <vector>
#include "../ENGINE_API_Macro.h"
#include "../Engine/Resources/AssetBase.h"
#include "DirectXMath.h"
#include "SceneComponent.h"

enum class RotationAxis
{
    X,
    Y,
    Z
};

class ENGINE_API SceneNode : public AssetBase
{
protected:
    static std::vector<SceneNode*> RootNodes;
    DirectX::XMMATRIX LocalMatrix = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX WorldMatrix = DirectX::XMMatrixIdentity();
    bool IsWorldMatrixDirty = true;
    bool IsStatic = false;
    std::string Name;
    SceneNode* Parent = nullptr;
    std::vector<SceneNode*> Children = {};
    std::vector<SceneComponent*> Components = {};
    
    SceneNode() = default;
    
    // Init() is required because a polymorphic inherited class 
    // cannot be added to mChildren at the time of initialization.
    // So Init() defers adding a child to the time of construction
    // in the inherited class. Ie. Entity() { Init(); }
    virtual void Init(std::string name, SceneNode* parent, DirectX::XMMATRIX localMatrix);

public:
    
    static void UpdateAll(float dt);
    static void DestroyAll();

    virtual ~SceneNode();
    
    virtual void Update(float dt);
    virtual void Reset();
    
    SceneNode* GetRootNode() { return Parent ? Parent->GetRootNode() : this; }
    std::vector<SceneNode*> GetChildren() const { return Children; }
    void AddChild(SceneNode* child);
    
    void UpdateWorldMatrix();
    void SetChildrenDirty();
    void SetDirty();
    
    DirectX::XMMATRIX GetLocalMatrix() const { return LocalMatrix; }
    DirectX::XMMATRIX GetWorldMatrix();
    
    void RotateEulerLocalAxis(RotationAxis axis, float angle);
    
    DirectX::XMFLOAT3 GetWorldPosition3f();
    DirectX::XMFLOAT3 GetLocalPosition3f();
    
    void SetLocalPosition(DirectX::XMFLOAT3 position);
    void SetWorldPosition(DirectX::XMFLOAT3 position);
    
    void SetLocalMatrix(DirectX::XMMATRIX localMatrix);
    void SetWorldMatrix(DirectX::XMMATRIX worldMatrix);
    
    void Serialize() override;
    void Deserialize() override;

private:
    void SerializeNode();
    void DeserializeNode(SceneNode* parent);
};
