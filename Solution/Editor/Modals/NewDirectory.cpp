#include "NewDirectory.h"
#include <filesystem>
#include "imgui.h"

namespace fs = std::filesystem;

NewDirectory& NewDirectory::GetInstance()
{
    static NewDirectory instance;
    return instance;
}

static int FilterLettersOnly(ImGuiInputTextCallbackData* data) {
    ImWchar c = data->EventChar;
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_'))) {
        return 1;
    }
    return 0;
}

void NewDirectory::Open(std::string startingPath)
{
    StartingPath = std::move(startingPath);
    PendingOpen = true;
}

void NewDirectory::Render()
{
    if (PendingOpen)
    {
        PendingOpen = false;
        ImGui::OpenPopup("NewDirectoryModal");
    }

    if (ImGui::BeginPopupModal("NewDirectoryModal", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Enter Directory name:");
        
        ImGui::InputText("##name", Buffer, IM_ARRAYSIZE(Buffer), ImGuiInputTextFlags_CallbackCharFilter, FilterLettersOnly);

        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            fs::create_directory(StartingPath + "/" + Buffer);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        
        ImGui::EndPopup();
    }
}
