#pragma once
#include <stdexcept>
#include <vector>
#include "Field.h"

class SceneNode;

enum ENGINE_API SceneComponentType : uint32_t
{
    MeshComponentType,
    // RigidBody,
    // BoxCollider,
    // etc.
    Count
};

static SceneComponentType ComponentTypeFromInt(uint32_t type)
{
    if (type < static_cast<uint32_t>(SceneComponentType::Count))
        return static_cast<SceneComponentType>(type);
    throw std::invalid_argument("Invalid component type");
}

// Scene components cannot be duplicated on the same scene node
class ENGINE_API SceneComponentBase
{
protected:
    std::vector<Field> Fields;
    SceneNode* Owner;
    SceneComponentBase(std::vector<Field> fields, SceneNode* owner) : Fields(std::move(fields)), Owner(owner) {}
    virtual ~SceneComponentBase() = default;
    
public:
    virtual SceneComponentType GetType() const = 0;
    virtual void Serialize(std::string& data);
    virtual void Deserialize(std::string& data, long& offset);
    
};

class ENGINE_API MeshComponent : public SceneComponentBase
{
public:
    // Deserialize constructor
    MeshComponent(SceneNode* owner) : SceneComponentBase({}, owner) {}
    
    // New component constructor
    MeshComponent(std::vector<Field> fields, SceneNode* owner) : SceneComponentBase(std::move(fields), owner) {}
    virtual SceneComponentType GetType() const override { return SceneComponentType::MeshComponentType; }
};

class ENGINE_API SceneComponentFactory
{
public:
    static SceneComponentBase* SceneComponentBuilder(SceneComponentType sceneComponentType)
    {
        switch (sceneComponentType)
        {
        case SceneComponentType::MeshComponentType:
            return new MeshComponent({}, nullptr);
        default:
            return nullptr;
        }
    }
};