#include "MeshAsset.h"
#include <filesystem>

MeshAsset::MeshAsset(std::string filePath) : DependentAssetBase("", AssetID::Generate(), {})
{
    std::filesystem::path path(filePath);
    Name = path.stem().generic_string();
    
    uint32_t numUVs = 0;
    
    // Load mesh
    // We need to load meshes which may include combining meshes for the same material on different UVs
    // This logic should be separate for objects that
    
    Fields.resize(numUVs);
}

void MeshAsset::Serialize(std::string& data)
{
    DependentAssetBase::Serialize(data);
    
    
    
}

void MeshAsset::Deserialize(std::string& data, long& offset)
{
    DependentAssetBase::Deserialize(data, offset);
}
