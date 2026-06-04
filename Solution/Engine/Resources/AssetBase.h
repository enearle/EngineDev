#pragma once

#include <vector>
#include <string>
#include "Field.h"
#include "../ENGINE_API_Macro.h"


class SceneNode;

class ENGINE_API AssetBase
{
protected:
    std::string Name = "";
    AssetID ID = {};
    AssetBase() = default;
    AssetBase(std::string name, AssetID id) : Name(std::move(name)), ID(id) {}
    virtual ~AssetBase() = default;
    
public:
    virtual void Serialize(std::string& data);
    virtual void Deserialize(std::string& data, long& offset);
    
    virtual const AssetID& GetID() const { return ID; }
    std::string GetAssetName() const { return Name; }
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

class ENGINE_API GPUAssetBase : public DependentAssetBase
{
protected:
    GPUAssetBase(std::string name, AssetID id, std::vector<Field> fields) : DependentAssetBase(std::move(name), id, std::move(fields)) {}
    virtual ~GPUAssetBase() = default;
public:
    virtual void UploadToGPU() = 0;
    virtual void FreeGPUResources() = 0;
};



