#include "SceneNode.h"

#include "../NoesisUILayer.h"
#include "../Resources/AssetSerializer.h"

using namespace DirectX;

std::vector<SceneNode*> SceneNode::RootNodes;

void SceneNode::Init(std::string name, SceneNode* parent, XMMATRIX localMatrix)
{
    if(parent)
    {
        Parent = parent;
        parent->AddChild(this);
    }
    else
        RootNodes.push_back(this);   
    
    LocalMatrix = localMatrix;
    Name = name;
}

void SceneNode::UpdateAll(float dt)
{
    for(auto root : RootNodes)
        root->Update(dt);
}

void SceneNode::DestroyAll()
{
    for(auto root : RootNodes)
        delete root;
    RootNodes.clear();
}

// Deserialization constructor does not initialize AssetBase because it is handled by Deserialize()
SceneNode::SceneNode(SceneNode* parent) : Parent(parent), AssetBase("", {})
{
    if (!parent)
    {
        RootNodes.push_back(this);
    }
    else
    {
        parent->AddChild(this);
    }
}

SceneNode::SceneNode(const std::string& name, AssetID assetId, SceneNode* parent) : AssetBase(name, assetId), Parent(parent)
{
    if (!parent)
    {
        RootNodes.push_back(this);
    }
    else
    {
        parent->AddChild(this);
    }
}

SceneNode::~SceneNode()
{
    // Detach from parent first to break circular references
    if (Parent)
    {
        auto& siblings = Parent->Children;
        auto it = std::find(siblings.begin(), siblings.end(), this);
        if (it != siblings.end())
        {
            siblings.erase(it);
        }
        Parent = nullptr;
    }
    
    // Now delete children
    std::vector<SceneNode*> childrenToDelete;
    childrenToDelete.swap(Children);
    
    for(auto child : childrenToDelete)
    {
        if(child && child != this) // Don't delete self!
        {
            child->Parent = nullptr;
            delete child;
        }
    }
}

void SceneNode::AddChild(SceneNode* child)
{
    Children.push_back(child);
}

void SceneNode::Update(float dt)
{
    for(auto child : Children)
        child->Update(dt);
}

void SceneNode::Reset()
{
    for (auto node : Children)
        node->Reset();
}

void SceneNode::UpdateWorldMatrix()
{
    if (!IsWorldMatrixDirty) return;
    
    WorldMatrix = Parent ? Parent->GetWorldMatrix() * LocalMatrix : LocalMatrix;
    IsWorldMatrixDirty = false;
}

void SceneNode::SetChildrenDirty()
{
    for (SceneNode* child : Children)
    {
        child->SetDirty();
        child->SetChildrenDirty();
    }
}

void SceneNode::SetDirty()
{
    IsWorldMatrixDirty = true;
}

DirectX::XMMATRIX SceneNode::GetWorldMatrix()
{
    UpdateWorldMatrix();
    return WorldMatrix;
}

XMFLOAT3 SceneNode::GetWorldPosition3f()
{
    XMFLOAT3 position;
    XMStoreFloat3(&position, GetWorldMatrix().r[3]);
    return position;
}

XMFLOAT3 SceneNode::GetLocalPosition3f()
{
    XMFLOAT3 position;
    XMStoreFloat3(&position, GetLocalMatrix().r[3]);
    return position;
}

void SceneNode::SetWorldPosition(DirectX::XMFLOAT3 position)
{
    XMMATRIX worldTransform = GetWorldMatrix();
    XMVECTOR posVec = XMLoadFloat3(&position);
    worldTransform.r[3] = XMVectorSetW(posVec, 1.0f);
    SetWorldMatrix(worldTransform);
}

void SceneNode::SetWorldMatrix(DirectX::XMMATRIX worldMatrix)
{
    LocalMatrix = Parent ? XMMatrixMultiply(worldMatrix, XMMatrixInverse(nullptr, Parent->GetWorldMatrix())) : worldMatrix;
    SetChildrenDirty();
}

void SceneNode::Serialize(std::string& data)
{
    AssetBase::Serialize(data);

    XMFLOAT4X4 transform;
    XMStoreFloat4x4(&transform, GetLocalMatrix());
    AssetSerializer::Write<XMFLOAT4X4>(data, transform);

    // Components (max 255)
    AssetSerializer::Write<uint8_t>(data, static_cast<uint8_t>(Components.size()));
    for (auto& component : Components)
    {
        AssetSerializer::Write<uint32_t>(data, component->GetType());
        component->Serialize(data);
    }

    // Children
    AssetSerializer::Write<uint32_t>(data, static_cast<uint32_t>(Children.size()));
    for (auto& child : Children)
        child->Serialize(data);
}

void SceneNode::Deserialize(std::string& data, long& offset)
{
    AssetBase::Deserialize(data, offset);

    XMFLOAT4X4 transform = AssetSerializer::Read<XMFLOAT4X4>(data, offset);
    LocalMatrix = XMLoadFloat4x4(&transform);

    uint8_t numComponents = AssetSerializer::Read<uint8_t>(data, offset);
    for (uint8_t i = 0; i < numComponents; i++)
    {
        uint32_t type = AssetSerializer::Read<uint32_t>(data, offset);

        SceneComponentBase* component = SceneComponentFactory::SceneComponentBuilder(ComponentTypeFromInt(type), this);
        if (component)
        {
            component->Deserialize(data, offset);
            Components.push_back(component);
        }
    }

    uint32_t numChildren = AssetSerializer::Read<uint32_t>(data, offset);
    for (uint32_t i = 0; i < numChildren; i++)
    {
        SceneNode* child = new SceneNode(this);
        child->Deserialize(data, offset);
    }
}

void SceneNode::SetLocalPosition(DirectX::XMFLOAT3 position)
{
    XMMATRIX newLocal = LocalMatrix;
    newLocal.r[3] = XMLoadFloat3(&position);
    SetLocalMatrix(newLocal);
}

void SceneNode::SetLocalMatrix(DirectX::XMMATRIX localMatrix)
{
    LocalMatrix = localMatrix;
    SetDirty();
    SetChildrenDirty();
}

void SceneNode::RotateEulerLocalAxis(RotationAxis axis, float angle)
{
    XMVECTOR axisVector;
    
    switch(axis)
    {
    case RotationAxis::X: 
        axisVector = XMVector3Normalize(GetLocalMatrix().r[0]); 
        break;
    case RotationAxis::Y: 
        axisVector = XMVector3Normalize(GetLocalMatrix().r[1]); 
        break;
    case RotationAxis::Z: 
        axisVector = XMVector3Normalize(GetLocalMatrix().r[2]); 
        break;
    }
    
    XMMATRIX rotation = XMMatrixRotationAxis(axisVector, angle);
    SetLocalMatrix(rotation * GetLocalMatrix());
}