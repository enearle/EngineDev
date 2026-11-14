#include "Camera.h"
Camera* Camera::ActiveCamera = nullptr;
bool Camera::BufferInitialized = false;
VPData Camera::ActiveVPData;
Uploader::BufferID Camera::BufferID = {};
VPData* Camera::MappedDataAddr = nullptr;
