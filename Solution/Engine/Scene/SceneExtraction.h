#pragma once
#include <vector>
#include "../ENGINE_API_Macro.h"

class Camera;
class Light;

// Copies what the renderer needs from the scene into Renderer::GetRenderScene(), once per frame.
// Walks every root SceneNode: each submesh of a MeshComponent becomes one render object.
class ENGINE_API SceneExtraction
{
public:
    static void ExtractFrame(const Camera* camera, const std::vector<const Light*>& lights);
};
