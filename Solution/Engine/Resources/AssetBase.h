#pragma once
#include <vector>
#include <string>
#include "Field.h"
#include "../ENGINE_API_Macro.h"


class SceneNode;

class ENGINE_API AssetBase
{
protected:
    std::string Name;
    AssetID ID;
    AssetBase(std::string name, AssetID id) : Name(std::move(name)), ID(id) {}
    virtual ~AssetBase() = default;
public:
    virtual void Serialize(std::string& data);
    virtual void Deserialize(std::string& data, long offset = 0);
    
};

class ENGINE_API DependencyBase : public AssetBase
{
protected:
    std::vector<Field> Fields;
    DependencyBase(std::string name, AssetID id, std::vector<Field> fields) : AssetBase(std::move(name), id), Fields(std::move(fields)) {}
    virtual ~DependencyBase() = default;
public:
    virtual void Serialize(std::string& data) override;
    virtual void Deserialize(std::string& data, long offset = 0) override;
    
};

class ENGINE_API SceneComponentBase : public DependencyBase
{
protected:
    SceneNode* Owner;
    SceneComponentBase(std::string name, AssetID id, std::vector<Field> fields, SceneNode* owner) : DependencyBase(std::move(name), id, std::move(fields)), Owner(owner) {}
    virtual ~SceneComponentBase() = default;
    
};

