#pragma once
#include <stdexcept>
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
    virtual void Deserialize(std::string& data, long& offset);
    
};

class ENGINE_API DependentAssetBase : public AssetBase
{
protected:
    std::vector<Field> Fields;
    DependentAssetBase(std::string name, AssetID id, std::vector<Field> fields) : AssetBase(std::move(name), id), Fields(std::move(fields)) {}
    virtual ~DependentAssetBase() = default;
public:
    virtual void Serialize(std::string& data) override;
    virtual void Deserialize(std::string& data, long& offset) override;
    
};



