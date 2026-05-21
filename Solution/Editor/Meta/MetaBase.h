#pragma once
#include <map>
#include <string>
#include <vector>

enum ResourceType : uint32_t
{
    Img1Ch,
    Img2Ch,
    Img3Ch,
    Img4Ch,
    Material,
    Mesh,
    SkinnedMesh
};

struct Field
{
    ResourceType Type;
    uint64_t ID;
};

class MetaBase
{
protected:
    // ID of next
    static uint64_t NewID;
    
    // Map of meta resources mati
    static std::map<uint64_t, std::string> MetaMap;
    
    // Serializes the meta map linking all resources, and current val of NewID 
    static void SerializeMainMetaList();
    static void LoadMainMetaList();
    
    // Creates/recreates a local meta file for the individual resource
    void SerializeLocalMetaFile();
    
    // Links to other resources within this resource
    std::vector<Field> Fields;
public:
    
    // Unique ID of resource
    uint64_t ID;
    
    // File location of resource
    std::string FileLocation;
    
    // Type of resource
    ResourceType Type;
    
    // Create new local meta file for imported resource
    MetaBase(ResourceType type, const std::string& fileLocation);
    
    // Load local meta file
    MetaBase(const std::string& fileLocation);
    
    // Change location of resource and meta file
    void UpdateLocation(const std::string& fileLocation);
    
};
