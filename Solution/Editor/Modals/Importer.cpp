#include "Importer.h"
#include "Resources/ResourceManager.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <imgui.h>
#include "../imfilebrowser.h"
#include <fstream>
namespace fs = std::filesystem;
ImGui::FileBrowser FILE_DIALOG;
std::string PATH_TO_IMPORT_TO;

ResourceType ParsePNGFile(const fs::path& path)
{
    std::ifstream fileStream(path, std::ios::binary);
    if (!fileStream)
        throw std::runtime_error("Failed to open file: " + path.string());

    fileStream.seekg(24);
    uint8_t bitDepth, colorType;
    fileStream.read(reinterpret_cast<char*>(&bitDepth), 1);
    fileStream.read(reinterpret_cast<char*>(&colorType), 1);
    if (!fileStream)
        throw std::runtime_error("Failed to read PNG header: " + path.string());

    int channels = 0;
    switch (colorType)
    {
    case 0: channels = 1; break; // Grayscale
    case 2: channels = 3; break; // RGB
    case 3: channels = 3; break; // Indexed (palette)
    case 4: channels = 2; break; // Grayscale + Alpha
    case 6: channels = 4; break; // RGBA
    default:
        throw std::runtime_error("Unsupported PNG color type: " + std::to_string(colorType));
    }

    // PNG only supports 8 or 16 bit depth
    if (bitDepth != 8 && bitDepth != 16)
        throw std::runtime_error("Unsupported PNG bit depth: " + std::to_string(bitDepth));

    // Encode as index into the 4-channel groups (1CH=0, 2CH=1, 3CH=2, 4CH=3)
    // then offset by bit depth group (8=0, 16=4)
    static constexpr ResourceType table[2][4] = {
        { ResourceType::Texture1CH8,  ResourceType::Texture2CH8,  ResourceType::Texture3CH8,  ResourceType::Texture4CH8  },
        { ResourceType::Texture1CH16, ResourceType::Texture2CH16, ResourceType::Texture3CH16, ResourceType::Texture4CH16 },
    };

    int depthIdx = (bitDepth == 16) ? 1 : 0;
    return table[depthIdx][channels - 1];
}

ResourceType ParseFBXFile(const fs::path& path)
{
    Assimp::Importer importer;
    // aiProcess_FindInvalidData is cheap; skip heavy flags like triangulate
    const aiScene* scene = importer.ReadFile(path.string(), 0);
    if (!scene)
        throw std::runtime_error(importer.GetErrorString());

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        if (scene->mMeshes[i]->mNumBones > 0)
            return ResourceType::MeshSkinned;
    }
    return ResourceType::Mesh;
}

ResourceType ParseExtension(const fs::path& path)
{
    if (path.extension() == ".png")
        return ParsePNGFile(path);
    else if (path.extension() == ".fbx")
        return ParseFBXFile(path);
    
    throw std::runtime_error("Unsupported import.");
}

Importer& Importer::GetInstance()
{
    static Importer instance;
    return instance;
}
void Importer::Open(std::string path)
{
    FILE_DIALOG.Open();
    FILE_DIALOG.SetDirectory("../Game/Assets");
    FILE_DIALOG.SetRootDirectory("../Game/Assets");
    FILE_DIALOG.SetTypeFilters({".png", ".fbx"});
    PATH_TO_IMPORT_TO = path;
}

void Importer::Render()
{
    FILE_DIALOG.Display();

    if (FILE_DIALOG.HasSelected())
    {
        ResourceManager::Import(FILE_DIALOG.GetSelected().string(), PATH_TO_IMPORT_TO, ParseExtension(FILE_DIALOG.GetSelected()));
        FILE_DIALOG.ClearSelected();
        FILE_DIALOG.Close();
    }
}




