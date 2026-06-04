#include "SceneExplorer.h"

#include <cstring>

#include "imgui.h"

#include "Scene/SceneNode.h"
#include "Resources/ResourceManager.h"
#include "Resources/SceneComponent.h"

namespace { SceneNode* SelectedNode = nullptr; }

static bool NodeHasDrawableMesh(SceneNode* node)
{
    MeshComponent* mc = static_cast<MeshComponent*>(node->GetComponent(MeshComponentType));
    return mc && mc->GetMeshAsset() != nullptr;
}

static void DrawNode(SceneNode* node)
{
    if (!node) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node->GetChildren().empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (node == SelectedNode)
        flags |= ImGuiTreeNodeFlags_Selected;

    bool drawable = NodeHasDrawableMesh(node);
    if (drawable) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.95f, 0.65f, 1.0f));
    bool open = ImGui::TreeNodeEx(node, flags, "%s", node->GetAssetName().c_str());
    if (drawable) ImGui::PopStyleColor();

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        SelectedNode = node;

    if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
    {
        for (SceneNode* child : node->GetChildren())
            DrawNode(child);
        ImGui::TreePop();
    }
}

void SceneExplorer::ShowSceneTree()
{
    ImGui::BeginChild("Scene Tree");

    // Drop strip at the top: accept SceneNode .asset payloads from FileExplorer.
    ImGui::InvisibleButton("##scene_drop_target", ImVec2(ImGui::GetContentRegionAvail().x, 6.f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ID"))
        {
            AssetID id;
            std::memcpy(&id, payload->Data, sizeof(AssetID));
            // GetAsset for a SceneNode constructs `new SceneNode()` (no parent),
            // whose deserialize ctor self-registers as a root in RootNodes.
            ResourceManager::GetAsset(id, ResourceType::SceneNode);
        }
        ImGui::EndDragDropTarget();
    }

    for (SceneNode* root : SceneNode::GetRootNodes())
        DrawNode(root);

    if (SelectedNode)
    {
        ImGui::Separator();
        ImGui::Text("Selected: %s", SelectedNode->GetAssetName().c_str());
    }

    ImGui::EndChild();
}
