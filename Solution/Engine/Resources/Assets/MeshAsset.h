#pragma once
#include "../AssetBase.h"

// Fields are for unique materials
// Unique materials represent unique draw calls
// Need a sophisticated batching system to minimize descriptor changes

class MeshAsset : DependentAssetBase
{
public:
    MeshAsset(std::string filePath);
    virtual void Serialize(std::string& data) override;
    virtual void Deserialize(std::string& data, long& offset) override;
};
