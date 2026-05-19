#include "FileExplorer.h"
#include "imgui.h"
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

#include "Modals/NewDirectory.h"


namespace fs = std::filesystem;

// Files with these extensions will be shown. Everything else is hidden.
static const std::vector<std::string> AllowedExtensions = {
    ".png"
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
    for (auto& entry : fs::directory_iterator(path, ec)) {
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
    for (const auto& d : dirs) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                                 | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (d.path() == selectedPath)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx(
            d.path().filename().string().c_str(), flags);

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            selectedPath = d.path();
            clicked = d.path();
        }
        
        if (ImGui::BeginPopupContextItem()) // Uses the ID of the previous item (the button)
        {
            if (ImGui::Selectable("New Folder")) { NewDirectory::GetInstance().Open(d.path().generic_string()); }
            if (ImGui::Selectable("Option 2")) { /* Action 2 */ }
    
            ImGui::EndPopup();
        }

        if (open) {
            auto sub = DrawDirectoryNode(d.path(), selectedPath);
            if (!sub.empty()) clicked = sub;
            ImGui::TreePop();
        }
    }

    // Render files (as leaves)
    for (const auto& f : files) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf
                                 | ImGuiTreeNodeFlags_NoTreePushOnOpen
                                 | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (f.path() == selectedPath)
            flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::TreeNodeEx(f.path().filename().string().c_str(), flags);
        if (ImGui::IsItemClicked()) {
            selectedPath = f.path();
            clicked = f.path();
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
    if (ImGui::CollapsingHeader(rootPath.filename().string().c_str(),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawDirectoryNode(rootPath, selectedPath);
                                }
    if (!selectedPath.empty()) {
        ImGui::Separator();
        ImGui::Text("Selected: %s", selectedPath.string().c_str());
    }
    ImGui::EndChild();
}