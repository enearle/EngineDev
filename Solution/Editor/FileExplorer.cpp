#include "FileExplorer.h"
#include "imgui.h"
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

#include "Modals/FileMove.h"
#include "Modals/Importer.h"
#include "Modals/NewDirectory.h"
#include "Resources/ResourceManager.h"


namespace fs = std::filesystem;

// Files with these extensions will be shown. Everything else is hidden.
static const std::vector<std::string> AllowedExtensions = {
    ".meta"
};

static bool IsWhitelisted(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    for (const auto& allowed : AllowedExtensions)
        if (ext == allowed) return true;
    return false;
}

// Returns the path the user clicked on, or empty path if nothing was clicked
// this frame.
static fs::path DrawDirectoryNode(const fs::path& path,
                                         fs::path& selectedPath)
{
    fs::path clicked;

    std::error_code ec;
    // Collect entries first so we can sort: directories first, then files.
    std::vector<fs::directory_entry> dirs, files;
    for (const fs::directory_entry& entry : fs::directory_iterator(path, ec)) {
        if (entry.is_directory(ec)) {
            dirs.push_back(entry);
        } else if (entry.is_regular_file(ec) && IsWhitelisted(entry.path())) {
            files.push_back(entry);
        }
    }

    auto byName = [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return a.path().filename().string() < b.path().filename().string();
    };
    std::sort(dirs.begin(),  dirs.end(),  byName);
    std::sort(files.begin(), files.end(), byName);

    // Render directories
    for (const fs::directory_entry& dir : dirs) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                                 | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (dir.path() == selectedPath)
            flags |= ImGuiTreeNodeFlags_Selected;
        
        bool open = ImGui::TreeNodeEx(dir.path().filename().string().c_str(), flags);
        
        if (ImGui::BeginDragDropSource())
        {
            std::string startPath = dir.path().generic_string();
            ImGui::SetDragDropPayload("PATH", startPath.c_str(), startPath.size());
            ImGui::EndDragDropSource();
        }
        
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PATH"))
            {
                std::string startPath(static_cast<const char*>(payload->Data), payload->DataSize);
                FileMove::GetInstance().Open(startPath, dir.path().generic_string());
            }
            ImGui::EndDragDropTarget();
        }
        
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            selectedPath = dir.path();
            clicked = dir.path();
        }
        
        if (ImGui::BeginPopupContextItem()) // Uses the ID of the previous item (the button)
        {
            if (ImGui::Selectable("New Folder")) { NewDirectory::GetInstance().Open(dir.path().generic_string()); }
            if (ImGui::Selectable("Import Asset")) { Importer::GetInstance().Open(dir.path().generic_string()); }
    
            ImGui::EndPopup();
        }

        if (open)
        {
            fs::path sub = DrawDirectoryNode(dir.path(), selectedPath);
            if (!sub.empty()) clicked = sub;
            ImGui::TreePop();
        }
    }

    // Render files (as leaves)
    for (const fs::directory_entry& file : files) 
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf
                                 | ImGuiTreeNodeFlags_NoTreePushOnOpen
                                 | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (file.path() == selectedPath)
            flags |= ImGuiTreeNodeFlags_Selected;
        
        // Read in meta file and get id
        std::string filePath = file.path().generic_string();
        AssetID id = ResourceManager::ReadMetaFile(filePath);
        
        // Convert path to meta file to path to asset and validate
        filePath = filePath.erase(filePath.length() - 5);
        if (!ResourceManager::ValidateAssetID(id, filePath))
            continue;
        
        std::string fileName = file.path().filename().string();
        fileName = fileName.erase(fileName.length() - 5);
        
        ImGui::TreeNodeEx(fileName.c_str(), flags);
        if (ImGui::IsItemClicked())
        {
            selectedPath = file.path();
            clicked = file.path();
        }

        if (ImGui::BeginDragDropSource())
        {
            // Only one payload survives per source; emit the asset ID for
            // consumers like SceneExplorer. Directories still drag a "PATH"
            // payload for the move flow.
            ImGui::SetDragDropPayload("ID", &id, sizeof(AssetID));
            ImGui::EndDragDropSource();
        }
    }

    return clicked;
}

// Public entry point
void FileExplorer::ShowFileTree(const std::string& root)
{
    fs::path rootPath(root);
    
    static fs::path selectedPath;

    ImGui::BeginChild("File Tree");
    if (ImGui::CollapsingHeader(rootPath.filename().string().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) 
        DrawDirectoryNode(rootPath, selectedPath);
    
    if (!selectedPath.empty()) 
    {
        ImGui::Separator();
        ImGui::Text("Selected: %s", selectedPath.string().c_str());
    }
    ImGui::EndChild();
}