#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

using AssetID = uint64_t;
namespace fs = std::filesystem;

enum class ResourceType : uint32_t {
    Texture2D,
    Material,
    Mesh
};

// Represents a dependency inside a baked asset
struct AssetDependency {
    ResourceType Type;
    AssetID ID;
};

// Information stored in the central registry/manifest
struct AssetMetadata {
    AssetID ID;
    ResourceType Type;
    fs::path SourcePath;  // Path to raw asset (Editor only)
    fs::path BakedPath;   // Path to engine-ready binary file
    std::vector<AssetDependency> Dependencies;
};

class ResourceManager {
private:
    // Central registry matching ID to asset metadata
    std::unordered_map<AssetID, AssetMetadata> m_Registry;
    
    // Quick lookup from source file path to ID (useful for the Editor)
    std::unordered_map<std::string, AssetID> m_PathToIDMap;
    
    // Tracks what is currently loaded into memory
    std::unordered_map<AssetID, void*> m_LoadedAssets; 

    AssetID GenerateUUID();

public:
    // --- MANIFEST MANAGEMENT ---
    void LoadRegistry(const fs::path& manifestPath);
    void SaveRegistry(const fs::path& manifestPath);

    // --- EDITOR / ASSET PIPELINE ---
    // Registers a new source file, assigns an ID, and sets up paths
    AssetID ImportSourceFile(const fs::path& sourcePath, ResourceType type);
    
    // Converts raw source data into optimized engine-ready binary data
    bool BakeAsset(AssetID id);

    // --- RUNTIME ENGINE ---
    // Loads the baked binary file into RAM/VRAM using the metadata registry
    void* LoadAsset(AssetID id);
    void UnloadAsset(AssetID id);
};
