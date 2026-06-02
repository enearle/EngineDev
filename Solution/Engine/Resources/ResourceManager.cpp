#include "ResourceManager.h"
#include <filesystem>
#include <fstream>

#include "AssetBase.h"
#include "Assets/MeshAsset.h"
#include "../Geometry/GeometryImport.h"
#include "../Scene/SceneNode.h"


namespace fs = std::filesystem;
std::map<AssetID, AssetRegistry> ResourceManager::Registry;
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

        AssetRegistry data;
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

std::optional<std::string> ResourceManager::ReadAllBytes(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) return std::nullopt;

    std::streamsize size = file.tellg();
    if (size < 0) return std::nullopt;

    std::string buffer;
    buffer.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (size > 0 && !file.read(buffer.data(), size))
        return std::nullopt;

    return buffer;
}

AssetBase* ResourceManager::GetAsset(const AssetID& id, ResourceType type)
{
    if (!ValidateAssetType(id, type)) return nullptr;

    auto data = ReadAllBytes(Registry[id].FilePath);
    if (!data) return nullptr;

    AssetBase* asset = nullptr;
    long offset = 0;
    switch (type)
    {
    case ResourceType::Mesh:
    case ResourceType::MeshSkinned:
        asset = new MeshAsset();
        break;
    case ResourceType::SceneNode:
        asset = new SceneNode();
        break;
    default:
        return nullptr;
    }

    asset->Deserialize(data.value(), offset);
    return asset;
}

void ResourceManager::CreateAsset(AssetBase* asset, ResourceType type, const std::string& directory)
{
    std::string path = directory + "/" + asset->GetAssetName() + ".asset";

    std::string data;
    asset->Serialize(data);

    std::ofstream outFile(path, std::ios::binary);
    outFile.write(data.data(), data.size());
    outFile.close();

    Registry[asset->GetID()] = AssetRegistry{ type, path };
    SaveRegistry();
}

AssetID ResourceManager::Import(const std::string& sourcePath, ResourceType type)
{
    // D8: no meta files. Dispatch by type.
    if (type == ResourceType::Mesh || type == ResourceType::MeshSkinned)
    {
        fs::path src(sourcePath);
        std::string name = src.stem().string();
        SceneNode* root = GeometryImport::CreateMeshGroup(sourcePath, name,
                                                         DirectX::XMMatrixIdentity(),
                                                         type == ResourceType::MeshSkinned);
        if (!root) return {};

        // D2: persist the SceneNode hierarchy itself alongside per-node MeshAssets.
        std::string destDir = src.parent_path().string();
        CreateAsset(root, ResourceType::SceneNode, destDir);
        return root->GetID();
    }

    // Textures and other types not yet implemented under the new flow.
    return {};
}

void ResourceManager::UpdateAssetPath(AssetID id, const std::string& newPath)
{
    Registry[id].FilePath = newPath;
    SaveRegistry();
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

bool ResourceManager::ValidateAssetType(const AssetID& id, ResourceType type)
{
    if (!Registry.contains(id)) return false;
    return Registry[id].Type == type;
}


