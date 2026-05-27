#pragma once
#include "ResourceManager.h"
#include "UUID.h"
#include "../ENGINE_API_Macro.h"

class ENGINE_API Field
{
    
    ResourceType Type;
    std::string Name;
public:
    Field(ResourceType type, std::string name) : Type(type), Name(std::move(name)) {}
    AssetID ID = {};

    ResourceType GetType() const { return Type; }
    const std::string& GetName() const { return Name; }
    const AssetID& GetID() const { return ID; }
    void SetID(const AssetID& id);
};
