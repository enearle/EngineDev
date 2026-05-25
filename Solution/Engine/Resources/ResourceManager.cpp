#include "ResourceManager.h"
#include <filesystem>
#include <fstream>


namespace fs = std::filesystem;
std::map<AssetID, AssetData> ResourceManager::Registry;
void ResourceManager::LoadRegistry()
{
    std::ifstream file(REGISTRY_FILE, std::ios::binary);
    if (!file) return;

    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (uint32_t i = 0; i < count; ++i)
    {
        AssetID uuid;
        file.read(reinterpret_cast<char*>(&uuid), sizeof(AssetID));

        AssetData data;
        file.read(reinterpret_cast<char*>(&data.Type), sizeof(ResourceType));

        uint32_t pathLen = 0;
        file.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
        data.FilePath.resize(pathLen);
        file.read(data.FilePath.data(), pathLen);

        Registry[uuid] = data;
    }
}

void ResourceManager::SaveRegistry()
{
    std::ofstream file(REGISTRY_FILE, std::ios::binary);

    uint32_t count = static_cast<uint32_t>(Registry.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& [uuid, data] : Registry)
    {
        file.write(reinterpret_cast<const char*>(&uuid), sizeof(AssetID));
        file.write(reinterpret_cast<const char*>(&data.Type), sizeof(ResourceType));

        uint32_t pathLen = static_cast<uint32_t>(data.FilePath.size());
        file.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        file.write(data.FilePath.data(), pathLen);
    }
}

AssetID ResourceManager::Import(const std::string& sourcePath, ResourceType type)
{
    AssetID newID = AssetID::Generate();

    AssetData data;
    data.FilePath = sourcePath;
    data.Type = type;
    Registry[newID] = data;
    SaveRegistry();
    CreateMetaFile(newID, sourcePath);
    return newID;
}

void ResourceManager::UpdateAssetPath(AssetID id, const std::string& newPath)
{
    Registry[id].FilePath = newPath;
    SaveRegistry();
}

void ResourceManager::CreateMetaFile(const AssetID& id, const std::string& filePath)
{
    std::ofstream outFile(filePath + ".meta", std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(&id), sizeof(AssetID));
}

AssetID ResourceManager::ReadMetaFile(const std::string& filePath)
{
    AssetID item;
    std::ifstream inFile(filePath, std::ios::binary);
    inFile.read(reinterpret_cast<char*>(&item), sizeof(AssetID));
    return item;
}

bool ResourceManager::ValidateAssetID(const AssetID& id, std::string& path)
{
    if (!Registry.contains(id)) return false;
    if (Registry[id].FilePath != path) return false;
    if (!fs::exists(path)) return false;
    
    return true;
}


