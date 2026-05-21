#include "ResourceManager.h"

AssetID ResourceManager::GenerateUUID()
{
}

void ResourceManager::LoadRegistry(const fs::path& manifestPath)
{
}

void ResourceManager::SaveRegistry(const fs::path& manifestPath)
{
}

AssetID ResourceManager::ImportSourceFile(const fs::path& sourcePath, ResourceType type) {
    std::string pathStr = sourcePath.string();
    if (m_PathToIDMap.contains(pathStr)) {
        return m_PathToIDMap[pathStr];
    }

    AssetID newID = GenerateUUID();
    
    AssetMetadata meta;
    meta.ID = newID;
    meta.Type = type;
    meta.SourcePath = sourcePath;
    
    // Baked assets are named by ID and grouped in a cache directory
    meta.BakedPath = fs::path("Saved/Cooked") / std::to_string(newID).append(".baked");

    m_Registry[newID] = meta;
    m_PathToIDMap[pathStr] = newID;

    return newID;
}

bool ResourceManager::BakeAsset(AssetID id) {
    const auto& meta = m_Registry[id];
    
    // 1. Read raw file from meta.SourcePath
    // 2. Compress/format data based on meta.Type (e.g., compress PNG to BC7 texture)
    // 3. Collect dependencies (e.g., if a Material, find the IDs of its textures)
    
    // 4. Write optimized binary to meta.BakedPath with a custom engine header:
    //    [Magic Number] [AssetType] [Dependency Count] [Dependency IDs...] [Raw Binary Data]
    
    return true; 
}

void* ResourceManager::LoadAsset(AssetID id)
{
}

void ResourceManager::UnloadAsset(AssetID id)
{
}
