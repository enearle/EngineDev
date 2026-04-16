#include "Camera.h"
Camera* Camera::ActiveCamera = nullptr;
bool Camera::BufferInitialized = false;
VPData Camera::ActiveVPData;
uint64_t Camera::BufferID = 0;
VPData* Camera::MappedDataAddr = nullptr;
