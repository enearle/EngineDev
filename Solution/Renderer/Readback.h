#pragma once
#include <functional>
#include "RENDERER_API_Macro.h"
#include "PassGraph.h"

struct ReadbackResult
{
    const uint8_t* Data = nullptr;      // Rows of RowPitch bytes; the first Width texels of each row are valid
    uint32_t RowPitch = 0;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint64_t RequestedFrame = 0;        // The frame whose contents were copied
    uint64_t DeliveredFrame = 0;        // The frame being started when the callback runs
};

using ReadbackCallback = std::function<void(const ReadbackResult&)>;

// Copies parts of graph images into CPU-readable buffers and hands them back once the GPU has
// finished the frame that recorded the copy. Never waits: completion is polled at the start of each
// frame. One per program.
//
// The copies run in one compute pass placed after every other non-overlay pass. An image is
// declared to that pass with Watch, once, so the pass is ordered after the image's writers.
class RENDERER_API Readback
{
public:
    static void Start();
    static void Shutdown();

    static void Watch(GraphImageID image);
    static void Unwatch(GraphImageID image);
    static void Request(GraphImageID image, const RHIStructures::ImageRegion& region, ReadbackCallback callback);

    // Fires the callbacks of every copy whose frame the GPU has finished. Called by the renderer at
    // the start of each frame.
    static void DeliverCompleted();
};
