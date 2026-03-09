#include "../RHI/Renderer.h"
#include "Common/Window.h"
#include "Common/RHI/RHIConstants.h"
#include "Common/RHI/Geometry/GeometryImport.h"

int main(int argc, char* argv[])
{
    Window* window = new Window(L"MyWindow", Win32, 1280, 720);
    
    Renderer::Start(window);
    
    std::vector<class Pipeline*> pipelines;
    pipelines.push_back(RHIConstants::PBRGeometryPipeline());
    std::vector<IOResource> inputResources = {*pipelines[0]->GetOutputResource()};
    pipelines.push_back(RHIConstants::DeferredLightingPipeline(&inputResources));
    
    RootNode meshRoot = GeometryImport::CreateMeshGroup("shells.fbx", "Shells", DirectX::XMMatrixIdentity());
    
    Renderer::Run();
    
    return Renderer::End();
}
