#include "FileMove.h"
#include <filesystem>
#include <imgui.h>

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
        
        std::string text = "Are you sure you would like to move " + start.filename().generic_string() 
                + " to " + end.filename().generic_string() + "?";
        
        ImGui::Text("%s", text.c_str());
        
        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            end += "/";
            end += start.filename().generic_string();
            
            fs::rename(start, end, ec);
            
            if (ec)
            {
                ImGui::OpenPopup("ErrorModal");
                return;
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