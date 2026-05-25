#pragma once

#include <string>
#include <map>
#include "UUID.h"
#include "../ENGINE_API_Macro.h"

static constexpr const char* REGISTRY_FILE = "registry.bin";

enum class ResourceType : uint32_t {
    Texture1CH8,
    Texture2CH8,
    Texture3CH8,
    Texture4CH8,
    Texture1CH16,
    Texture2CH16,
    Texture3CH16,
    Texture4CH16,
    Material,
    Mesh,
    MeshSkinned,
    GameObject
};

struct AssetData {
    ResourceType Type;
    std::string FilePath;
};

class ENGINE_API ResourceManager {
private:

    static std::map<AssetID, AssetData> Registry;
    static void CreateMetaFile(const AssetID& id, const std::string& filePath);
    
public:

    static void LoadRegistry();
    static void SaveRegistry();
    
    static AssetID Import(const std::string& sourcePath, ResourceType type);
    static void UpdateAssetPath(AssetID id, const std::string& newPath);
    static AssetID ReadMetaFile(const std::string& filePath);
    static bool ValidateAssetID(const AssetID& id, std::string& path);
    // Handle asset loading and baking later, maybe with hooks for runtimes
    
    // Validate AssetUUIDs point to existing file, also needed
    
    // Tracks what is currently loaded into memory
    ///std::unordered_map<AssetID, void*> LoadedAssets; 
};
