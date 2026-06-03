#include "FileMove.h"
#include <filesystem>
#include <imgui.h>
#include "Resources/ResourceManager.h"
#include "Resources/UUID.h"

namespace fs = std::filesystem;

FileMove& FileMove::GetInstance()
{
    static FileMove instance;
    return instance;
}

void FileMove::Open(std::string startingPath, std::string destinationPath)
{
    StartingPath = std::move(startingPath);
    DestinationPath = std::move(destinationPath);
    PendingOpen = true;
}

void FileMove::Render()
{
    if (PendingOpen)
    {
        PendingOpen = false;
        ImGui::OpenPopup("FileMoveModal");
    }
    
    if (ImGui::BeginPopupModal("FileMoveModal", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        fs::path start(StartingPath);
        fs::path end(DestinationPath);
        
        std::string fileName = start.filename().generic_string();
        if (start.filename().extension() == ".meta")
            fileName = fileName.erase(fileName.length() - 5);
        
        std::string text = "Are you sure you would like to move " + fileName 
                + " to " + end.filename().generic_string() + "?";
        
        ImGui::Text("%s", text.c_str());
        
        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            AssetID id = ResourceManager::ReadMetaFile(StartingPath);
            
            end += "/";
            end += start.filename().generic_string();
            
            // Moving meta asset or directory
            fs::rename(start, end, ec);
            
            if (ec)
            {
                ImGui::OpenPopup("ErrorModal");
                return;
            }
            
            // Removing '.meta' to move the base asset
            if (start.filename().extension() == ".meta")
            {
                std::string startString = start.generic_string();
                std::string endString = end.generic_string();
            
                startString = startString.erase(startString.length() - 5);
                endString = endString.erase(endString.length() - 5);
            
                start = startString;
                end = endString;
            
                fs::rename(start, end, ec);
            
                if (ec)
                {
                    ImGui::OpenPopup("ErrorModal");
                    return;
                }
            
                ResourceManager::UpdateAssetPath(id, endString);
            }
            else
            {
                // Updates existing meta files for each asset in the sub directories
                for (const fs::directory_entry& entry : fs::recursive_directory_iterator(end))
                {
                    if (!entry.is_regular_file() || entry.path().extension() == ".meta")
                        continue;

                    std::string assetPath = entry.path().generic_string();
                    std::string metaPath = assetPath + ".meta";

                    if (fs::exists(metaPath))
                    {
                        AssetID assetId = ResourceManager::ReadMetaFile(metaPath);
                        ResourceManager::UpdateAssetPath(assetId, assetPath);
                    }
                }
            }
            
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        
        
        ImGui::EndPopup();
    }
    
    if (ImGui::BeginPopupModal("ErrorModal", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text(("Error moving file: " + ec.message()).c_str());
        
        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}