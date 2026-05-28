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

    // Transform
    XMFLOAT4X4 transform;
    XMStoreFloat4x4(&transform, GetLocalMatrix());

    char matrix[64];
    for (uint32_t i = 0; i < 16; ++i)
    {
        uint32_t bits;
        memcpy(&bits, &transform.m[i / 4][i % 4], sizeof(uint32_t));
        matrix[i * 4]     = (bits >> 24) & 0xFF;
        matrix[i * 4 + 1] = (bits >> 16) & 0xFF;
        matrix[i * 4 + 2] = (bits >> 8)  & 0xFF;
        matrix[i * 4 + 3] =  bits        & 0xFF;
    }
    data.append(matrix, 64);

    // Components (max 255)
    data += static_cast<char>(Components.size());
    for (auto& component : Components)
    {
        uint32_t type = component->GetType();
        char typeData[4];
        typeData[0] = (type >> 24) & 0xFF;
        typeData[1] = (type >> 16) & 0xFF;
        typeData[2] = (type >> 8)  & 0xFF;
        typeData[3] =  type        & 0xFF;
        data.append(typeData);
        component->Serialize(data);
    }

    // Children
    uint32_t numChildren = static_cast<uint32_t>(Children.size());
    char children[4];
    children[0] = (numChildren >> 24) & 0xFF;
    children[1] = (numChildren >> 16) & 0xFF;
    children[2] = (numChildren >> 8)  & 0xFF;
    children[3] =  numChildren        & 0xFF;
    data.append(children, 4);

    for (auto& child : Children)
        child->Serialize(data);
}

void SceneNode::Deserialize(std::string& data, long& offset)
{
    AssetBase::Deserialize(data, offset);
    
    // Transform
    XMFLOAT4X4 transform;
    std::string matrix = data.substr(offset, 64);
    for (uint32_t i = 0; i < 16; i++)
    {
        std::string value = matrix.substr(i * 4, 4);
        uint32_t bits = static_cast<uint32_t>(static_cast<uint8_t>(value[0])) << 24 |
                        static_cast<uint32_t>(static_cast<uint8_t>(value[1])) << 16 |
                        static_cast<uint32_t>(static_cast<uint8_t>(value[2])) << 8  |
                        static_cast<uint32_t>(static_cast<uint8_t>(value[3]));
        
        memcpy(&transform.m[i / 4][i % 4], &bits, sizeof(float));
    }
    
    LocalMatrix = XMLoadFloat4x4(&transform);
    offset += 64;
    
    // Components
    uint8_t numComponents = static_cast<uint8_t>(data[offset++]);
    for (uint8_t i = 0; i < numComponents; i++)
    {
        std::string typeData = data.substr(offset, 4);
        offset += 4;
        
        uint32_t type = static_cast<uint32_t>(static_cast<uint8_t>(typeData[0])) << 24 |
                        static_cast<uint32_t>(static_cast<uint8_t>(typeData[1])) << 16 |
                        static_cast<uint32_t>(static_cast<uint8_t>(typeData[2])) << 8  |
                        static_cast<uint32_t>(static_cast<uint8_t>(typeData[3]));
        

        SceneComponentBase* component = SceneComponentFactory::SceneComponentBuilder(ComponentTypeFromInt(type));
        if (component)
        {
            component->Deserialize(data, offset);
            Components.push_back(component);
        }
    }
    
    // Children
    std::string numChildData = data.substr(offset, 4);
    offset += 4;
    uint32_t numChildren = static_cast<uint32_t>(static_cast<uint8_t>(numChildData[0])) << 24 |
                           static_cast<uint32_t>(static_cast<uint8_t>(numChildData[1])) << 16 |
                           static_cast<uint32_t>(static_cast<uint8_t>(numChildData[2])) << 8  |
                           static_cast<uint32_t>(static_cast<uint8_t>(numChildData[3]));
    
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