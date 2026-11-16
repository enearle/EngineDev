#pragma once
#include <string>

class Camera;

// -paging-test: a plane textured with a 16,384 x 16,384 page file, built on first use, seen from a
// scripted camera path (the whole plane, a dolly to a close-up, a pan and a pull back). Up to 300
// fills are read back from the atlas and compared with their tiles. The game quits when the path
// ends, and the result is printed at exit. With an image path, the image is cut into a page file
// with PageFile::Build and shown instead of the pattern.
class PagingTest
{
public:
    static void Start(const std::string& imagePath);
    static void Update(float dt, Camera* camera);
    static void PrintResults();
    static void Shutdown();

    // The "Paging" overlay panel, for any scene.
    static void DrawPanel();
};
