#pragma once

#ifdef RHI_EXPORTS
#define RHI_API __declspec(dllexport)
#else
#define RHI_API __declspec(dllimport)
#endif

class RHI_API Renderer
{
public:
    static int run();
};